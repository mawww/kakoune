#include "input_handler.hh"

#include "context.hh"
#include "editor.hh"
#include "register_manager.hh"
#include "event_manager.hh"
#include "utf8.hh"
#include "color_registry.hh"

#include <unordered_map>

namespace Kakoune
{

extern std::unordered_map<Key, std::function<void (Context& context)>> keymap;

class InputMode
{
public:
    InputMode(InputHandler& input_handler) : m_input_handler(input_handler) {}
    virtual ~InputMode() {}
    InputMode(const InputMode&) = delete;
    InputMode& operator=(const InputMode&) = delete;

    virtual void on_key(const Key& key) = 0;
    Context& context() const { return m_input_handler.context(); }

    using Insertion = InputHandler::Insertion;
    Insertion& last_insert() { return m_input_handler.m_last_insert; }

protected:
    void reset_normal_mode();
private:
    InputHandler& m_input_handler;
};

namespace InputModes
{

static constexpr std::chrono::milliseconds idle_timeout{100};

class Normal : public InputMode
{
public:
    Normal(InputHandler& input_handler)
        : InputMode(input_handler),
          m_idle_timer{Clock::now() + idle_timeout, [this](Timer& timer) {
              context().hooks().run_hook("NormalIdle", "", context());
          }}
    {
        context().hooks().run_hook("NormalBegin", "", context());
    }

    ~Normal()
    {
        context().hooks().run_hook("NormalEnd", "", context());
    }

    void on_key(const Key& key) override
    {
        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            m_count = m_count * 10 + key.key - '0';
        else
        {
            auto it = keymap.find(key);
            if (it != keymap.end())
            {
                context().numeric_param() = m_count;
                it->second(context());
            }
            m_count = 0;
        }
        context().hooks().run_hook("NormalKey", key_to_str(key), context());
        m_idle_timer.set_next_date(Clock::now() + idle_timeout);
    }

private:
    int m_count = 0;
    Timer m_idle_timer;
};

class LineEditor
{
public:
    void handle_key(const Key& key)
    {
        if (key == Key::Left or
            key == Key{Key::Modifiers::Control, 'b'})
        {
            if (m_cursor_pos > 0)
                --m_cursor_pos;
        }
        else if (key == Key::Right or
                 key == Key{Key::Modifiers::Control, 'f'})
        {
            if (m_cursor_pos < m_line.char_length())
                ++m_cursor_pos;
        }
        else if (key == Key::Home)
            m_cursor_pos = 0;
        else if (key == Key::End)
            m_cursor_pos = m_line.char_length();
        else if (key == Key::Backspace)
        {
            if (m_cursor_pos != 0)
            {
                m_line = m_line.substr(0, m_cursor_pos - 1)
                       + m_line.substr(m_cursor_pos);

                --m_cursor_pos;
            }
        }
        else
        {
            m_line = m_line.substr(0, m_cursor_pos) + codepoint_to_str(key.key)
                   + m_line.substr(m_cursor_pos);
            ++m_cursor_pos;
        }
    }

    void insert(const String& str)
    {
        insert_from(m_cursor_pos, str);
    }

    void insert_from(CharCount start, const String& str)
    {
        assert(start <= m_cursor_pos);
        m_line = m_line.substr(0, start) + str
               + m_line.substr(m_cursor_pos);
       m_cursor_pos = start + str.char_length();
    }

    void reset(String line)
    {
        m_line = std::move(line);
        m_cursor_pos = m_line.char_length();
    }

    const String& line() const { return m_line; }
    CharCount cursor_pos() const { return m_cursor_pos; }
private:
    CharCount      m_cursor_pos = 0;
    String         m_line;
};

class Menu : public InputMode
{
public:
    Menu(InputHandler& input_handler, const memoryview<String>& choices,
         MenuCallback callback)
        : InputMode(input_handler),
          m_callback(callback), m_choices(choices.begin(), choices.end()),
          m_selected(m_choices.begin())
    {
        DisplayCoord menu_pos{ context().ui().dimensions().line, 0_char };
        ColorRegistry& colreg = ColorRegistry::instance();
        context().ui().menu_show(choices, menu_pos, colreg["MenuForeground"],
                                 colreg["MenuBackground"], MenuStyle::Prompt);
    }

    void on_key(const Key& key) override
    {
        auto match_filter = [this](const String& str) {
            return boost::regex_match(str.begin(), str.end(), m_filter);
        };

        if (key == Key(Key::Modifiers::Control, 'm'))
        {
            context().ui().menu_hide();
            context().ui().print_status("");
            reset_normal_mode();
            int selected = m_selected - m_choices.begin();
            m_callback(selected, MenuEvent::Validate, context());
            return;
        }
        else if (key == Key::Escape or key == Key{ Key::Modifiers::Control, 'c' })
        {
            if (m_edit_filter)
            {
                m_edit_filter = false;
                m_filter = boost::regex(".*");
                m_filter_editor.reset("");
                context().ui().print_status("");
            }
            else
            {
                context().ui().menu_hide();
                reset_normal_mode();
                int selected = m_selected - m_choices.begin();
                m_callback(selected, MenuEvent::Abort, context());
            }
        }
        else if (key == Key::Down or
                 key == Key(Key::Modifiers::Control, 'i') or
                 key == Key(Key::Modifiers::Control, 'n') or
                 key == Key(Key::Modifiers::None, 'j'))
        {
            auto it = std::find_if(m_selected+1, m_choices.end(), match_filter);
            if (it == m_choices.end())
                it = std::find_if(m_choices.begin(), m_selected, match_filter);
            select(it);
        }
        else if (key == Key::Up or
                 key == Key::BackTab or
                 key == Key(Key::Modifiers::Control, 'p') or
                 key == Key(Key::Modifiers::None, 'k'))
        {
            ChoiceList::const_reverse_iterator selected(m_selected+1);
            auto it = std::find_if(selected+1, m_choices.rend(), match_filter);
            if (it == m_choices.rend())
                it = std::find_if(m_choices.rbegin(), selected, match_filter);
            select(it.base()-1);
        }
        else if (key == '/' and not m_edit_filter)
        {
            m_edit_filter = true;
        }
        else if (m_edit_filter)
        {
            m_filter_editor.handle_key(key);

            auto search = ".*" + m_filter_editor.line() + ".*";
            m_filter = boost::regex(search.begin(), search.end());
            auto it = std::find_if(m_selected, m_choices.end(), match_filter);
            if (it == m_choices.end())
                it = std::find_if(m_choices.begin(), m_selected, match_filter);
            select(it);
        }

        if (m_edit_filter)
            context().ui().print_status("/" + m_filter_editor.line(),
                                      m_filter_editor.cursor_pos() + 1);
   }

private:
    MenuCallback m_callback;

    using ChoiceList = std::vector<String>;
    const ChoiceList m_choices;
    ChoiceList::const_iterator m_selected;

    void select(ChoiceList::const_iterator it)
    {
        m_selected = it;
        int selected = m_selected - m_choices.begin();
        context().ui().menu_select(selected);
        m_callback(selected, MenuEvent::Select, context());
    }

    boost::regex m_filter = boost::regex(".*");
    bool         m_edit_filter = false;
    LineEditor   m_filter_editor;
};

String common_prefix(const memoryview<String>& strings)
{
    String res;
    if (strings.empty())
        return res;
    res = strings[0];
    for (auto& str : strings)
    {
        ByteCount len = std::min(res.length(), str.length());
        ByteCount common_len = 0;
        while (common_len < len and str[common_len] == res[common_len])
            ++common_len;
        if (common_len != res.length())
            res = res.substr(0, common_len);
    }
    return res;
}

class Prompt : public InputMode
{
public:
    Prompt(InputHandler& input_handler, const String& prompt,
           Completer completer, PromptCallback callback)
        : InputMode(input_handler), m_prompt(prompt),
          m_completer(completer), m_callback(callback)
    {
        m_history_it = ms_history[m_prompt].end();
        context().ui().print_status(m_prompt, m_prompt.char_length());
    }

    void on_key(const Key& key) override
    {
        std::vector<String>& history = ms_history[m_prompt];
        const String& line = m_line_editor.line();

        if (m_insert_reg)
        {
            String reg = RegisterManager::instance()[key.key].values(context())[0];
            m_line_editor.insert(reg);
            m_insert_reg = false;
        }
        else if (key == Key{Key::Modifiers::Control, 'm'}) // enter
        {
            if (not line.empty())
            {
                std::vector<String>::iterator it;
                while ((it = find(history, line)) != history.end())
                    history.erase(it);
                history.push_back(line);
            }
            context().ui().print_status("");
            context().ui().menu_hide();
            reset_normal_mode();
            // call callback after reset_normal_mode so that callback
            // may change the mode
            m_callback(line, PromptEvent::Validate, context());
            return;
        }
        else if (key == Key::Escape or key == Key { Key::Modifiers::Control, 'c' })
        {
            context().ui().print_status("");
            context().ui().menu_hide();
            reset_normal_mode();
            m_callback(line, PromptEvent::Abort, context());
            return;
        }
        else if (key == Key{Key::Modifiers::Control, 'r'})
        {
            m_insert_reg = true;
        }
        else if (key == Key::Up or
                 key == Key{Key::Modifiers::Control, 'p'})
        {
            if (m_history_it != history.begin())
            {
                if (m_history_it == history.end())
                   m_prefix = line;
                auto it = m_history_it;
                // search for the previous history entry matching typed prefix
                ByteCount prefix_length = m_prefix.length();
                do
                {
                    --it;
                    if (it->substr(0, prefix_length) == m_prefix)
                    {
                        m_history_it = it;
                        m_line_editor.reset(*it);
                        break;
                    }
                } while (it != history.begin());
            }
        }
        else if (key == Key::Down or // next
                 key == Key{Key::Modifiers::Control, 'n'})
        {
            if (m_history_it != history.end())
            {
                ByteCount prefix_length = m_prefix.length();
                // search for the next history entry matching typed prefix
                ++m_history_it;
                while (m_history_it != history.end() and
                       m_history_it->substr(0, prefix_length) != m_prefix)
                    ++m_history_it;

                if (m_history_it != history.end())
                    m_line_editor.reset(*m_history_it);
                else
                    m_line_editor.reset(m_prefix);
            }
        }
        else if (key == Key(Key::Modifiers::Control, 'i') or // tab completion
                 key == Key::BackTab)
        {
            const bool reverse = (key == Key::BackTab);
            CandidateList& candidates = m_completions.candidates;
            // first try, we need to ask our completer for completions
            if (m_current_completion == -1)
            {
                m_completions = m_completer(context(), line,
                                            line.byte_count_to(m_line_editor.cursor_pos()));
                if (candidates.empty())
                    return;

                context().ui().menu_hide();
                DisplayCoord menu_pos{ context().ui().dimensions().line, 0_char };
                ColorRegistry& colreg = ColorRegistry::instance();
                context().ui().menu_show(candidates, menu_pos, colreg["MenuForeground"],
                                         colreg["MenuBackground"], MenuStyle::Prompt);

                bool use_common_prefix = context().options()["complete_prefix"].get<bool>();
                String prefix = use_common_prefix ? common_prefix(candidates) : String();
                if (m_completions.end - m_completions.start > prefix.length())
                    prefix = line.substr(m_completions.start,
                                         m_completions.end - m_completions.start);

                auto it = find(candidates, prefix);
                if (it == candidates.end())
                {
                    m_current_completion = use_common_prefix ? candidates.size() : 0;
                    candidates.push_back(std::move(prefix));
                }
                else
                    m_current_completion = use_common_prefix ? it - candidates.begin() : 0;
            }
            else if (not reverse and ++m_current_completion >= candidates.size())
                m_current_completion = 0;
            else if (reverse and --m_current_completion < 0)
                m_current_completion = candidates.size()-1;

            const String& completion = candidates[m_current_completion];
            context().ui().menu_select(m_current_completion);

            m_line_editor.insert_from(line.char_count_to(m_completions.start),
                                      completion);

            // when we have only one completion candidate, make next tab complete
            // from the new content.
            if (candidates.size() == 1)
                m_current_completion = -1;
        }
        else
        {
            context().ui().menu_hide();
            m_current_completion = -1;
            m_line_editor.handle_key(key);
        }
        context().ui().print_status(m_prompt + line,
                                  m_prompt.char_length() + m_line_editor.cursor_pos());
        m_callback(line, PromptEvent::Change, context());
    }

private:
    PromptCallback m_callback;
    Completer      m_completer;
    const String   m_prompt;
    Completions    m_completions;
    int            m_current_completion = -1;
    String         m_prefix;
    LineEditor     m_line_editor;
    bool           m_insert_reg = false;

    static std::unordered_map<String, std::vector<String>> ms_history;
    std::vector<String>::iterator m_history_it;
};
std::unordered_map<String, std::vector<String>> Prompt::ms_history;

class NextKey : public InputMode
{
public:
    NextKey(InputHandler& input_handler, KeyCallback callback)
        : InputMode(input_handler), m_callback(callback) {}

    void on_key(const Key& key) override
    {
        reset_normal_mode();
        m_callback(key, context());
   }

private:
    KeyCallback m_callback;
};

struct BufferCompletion
{
    BufferIterator begin;
    BufferIterator end;
    CandidateList  candidates;

    bool is_valid() const { return begin.is_valid() and not candidates.empty(); }
};

static BufferCompletion complete_word(const BufferIterator& pos)
{
   if (pos.is_begin() or not is_word(*utf8::previous(pos)))
       return {};

    BufferIterator end = pos;
    BufferIterator begin = end-1;
    while (not begin.is_begin() and is_word(*begin))
        --begin;
    if (not is_word(*begin))
        ++begin;

    const Buffer& buffer = pos.buffer();
    String ex = R"(\<\Q)" + buffer.string(begin, end) + R"(\E\w+\>)";
    Regex re(ex.begin(), ex.end());
    using RegexIt = boost::regex_iterator<BufferIterator>;

    CandidateList result;
    for (RegexIt it(buffer.begin(), buffer.end(), re), re_end; it != re_end; ++it)
    {
        auto& match = (*it)[0];
        if (match.first <= pos and pos < match.second)
            continue;

        String content = buffer.string(match.first, match.second);
        if (not contains(result, content))
            result.emplace_back(std::move(content));
    }
    std::sort(result.begin(), result.end());
    return { begin, end, std::move(result) };
}

static BufferCompletion complete_opt(const BufferIterator& pos, OptionManager& options)
{
    using StringList = std::vector<String>;
    const StringList& opt = options["completions"].get<StringList>();
    if (opt.empty())
        return {};

    auto& desc = opt[0];
    static const Regex re(R"((\d+):(\d+)(?:\+(\d+))?@(\d+))");
    boost::smatch match;
    if (boost::regex_match(desc.begin(), desc.end(), match, re))
    {
        LineCount line   = str_to_int({match[1].first, match[1].second}) - 1;
        ByteCount column = str_to_int({match[2].first, match[2].second}) - 1;

        BufferIterator end = pos;
        if (match[3].matched)
        {
            ByteCount len = str_to_int({match[3].first, match[3].second});
            end = pos + len;
        }
        int timestamp = str_to_int(String(match[4].first, match[4].second));

        if (timestamp == pos.buffer().timestamp() and line == pos.line() and column == pos.column())
            return { pos, end, { opt.begin() + 1, opt.end() } };
    }
    return {};
}

class BufferCompleter : public OptionManagerWatcher
{
public:
    BufferCompleter(const Context& context)
        : m_context(context)
    {
        m_context.options().register_watcher(*this);
    }
    ~BufferCompleter()
    {
        m_context.options().unregister_watcher(*this);
    }
    BufferCompleter(const BufferCompleter&) = delete;
    BufferCompleter& operator=(const BufferCompleter&) = delete;

    void select(int offset)
    {
        if (not setup_ifn())
            return;

        m_current_candidate = (m_current_candidate + offset) % (int)m_matching_candidates.size();
        if (m_current_candidate < 0)
            m_current_candidate += m_matching_candidates.size();
        const String& candidate = m_matching_candidates[m_current_candidate];

        m_context.buffer().erase(m_completions.begin, m_completions.end);
        m_context.buffer().insert(m_completions.begin, candidate);
        m_completions.end = m_completions.begin + candidate.length();
        m_context.ui().menu_select(m_current_candidate);
    }

    void update()
    {
        if (m_completions.is_valid())
        {
            ByteCount longest_completion = 0;
            for (auto& candidate : m_completions.candidates)
                 longest_completion = std::max(longest_completion, candidate.length());

            BufferIterator cursor = m_context.editor().main_selection().last();
            if (cursor > m_completions.begin and ByteCount{(int)(cursor - m_completions.begin)} < longest_completion)
            {
                String prefix = m_context.buffer().string(m_completions.begin, cursor);
                m_matching_candidates.clear();
                for (auto& candidate : m_completions.candidates)
                {
                    if (candidate.substr(0, prefix.length()) == prefix)
                        m_matching_candidates.push_back(candidate);
                }
                if (not m_matching_candidates.empty())
                {
                    m_context.ui().menu_hide();
                    m_current_candidate = m_matching_candidates.size();
                    m_completions.end = cursor;
                    menu_show();
                    m_matching_candidates.push_back(prefix);
                    return;
                }
            }
        }
        reset();
        select(0);
    }

    void reset()
    {
        m_completions = BufferCompletion{};
        m_context.ui().menu_hide();
    }
private:
    void on_option_changed(const Option& opt) override
    {
        if (opt.name() == "completions")
        {
            reset();
            select(0);
        }
    }

    void menu_show()
    {
        DisplayCoord menu_pos = m_context.window().display_position(m_completions.begin);

        ColorRegistry& colreg = ColorRegistry::instance();
        m_context.ui().menu_show(m_matching_candidates, menu_pos,
                                 colreg["MenuForeground"],
                                 colreg["MenuBackground"],
                                 MenuStyle::Inline);
        m_context.ui().menu_select(m_current_candidate);
    }

    bool setup_ifn()
    {
        if (not m_completions.is_valid())
        {
            BufferIterator cursor = m_context.editor().main_selection().last();
            m_completions = complete_opt(cursor, m_context.options());
            if (not m_completions.is_valid())
                m_completions = complete_word(cursor);
            if (not m_completions.is_valid())
                return false;

            assert(cursor >= m_completions.begin);

            m_matching_candidates = m_completions.candidates;
            m_current_candidate = m_matching_candidates.size();
            menu_show();
            m_matching_candidates.push_back(m_context.buffer().string(m_completions.begin, m_completions.end));
        }
        return true;
    }

    const Context&   m_context;
    BufferCompletion m_completions;
    CandidateList    m_matching_candidates;
    int              m_current_candidate = -1;
};

class Insert : public InputMode
{
public:
    Insert(InputHandler& input_handler, InsertMode mode)
        : InputMode(input_handler),
          m_inserter(context().editor(), mode),
          m_completer(context()),
          m_idle_timer{Clock::now() + idle_timeout,
                       [this](Timer& timer) {
                           context().hooks().run_hook("InsertIdle", "", context());
                           m_completer.update();
                       }}
    {
        last_insert().first = mode;
        last_insert().second.clear();
        context().hooks().run_hook("InsertBegin", "", context());
    }

    void on_key(const Key& key) override
    {
        last_insert().second.push_back(key);
        if (m_insert_reg)
        {
            if (key.modifiers == Key::Modifiers::None)
                m_inserter.insert(RegisterManager::instance()[key.key].values(context()));
            m_insert_reg = false;
            return;
        }
        bool update_completions = true;
        bool moved = false;
        if (key == Key::Escape or key == Key{ Key::Modifiers::Control, 'c' })
        {
            context().hooks().run_hook("InsertEnd", "", context());
            m_completer.reset();
            reset_normal_mode();
        }
        else if (key == Key::Backspace)
            m_inserter.erase();
        else if (key == Key::Left)
        {
            m_inserter.move_cursors(-1_char);
            moved = true;
        }
        else if (key == Key::Right)
        {
            m_inserter.move_cursors(1_char);
            moved = true;
        }
        else if (key == Key::Up)
        {
            m_inserter.move_cursors(-1_line);
            moved = true;
        }
        else if (key == Key::Down)
        {
            m_inserter.move_cursors(1_line);
            moved = true;
        }
        else if (key.modifiers == Key::Modifiers::None)
        {
            m_inserter.insert(codepoint_to_str(key.key));
            context().hooks().run_hook("InsertKey", key_to_str(key), context());
        }
        else if (key == Key{ Key::Modifiers::Control, 'r' })
            m_insert_reg = true;
        else if ( key == Key{ Key::Modifiers::Control, 'm' })
            m_inserter.insert(String() + '\n');
        else if ( key == Key{ Key::Modifiers::Control, 'i' })
            m_inserter.insert(String() + '\t');
        else if ( key == Key{ Key::Modifiers::Control, 'n' })
        {
            m_completer.select(1);
            update_completions = false;
        }
        else if ( key == Key{ Key::Modifiers::Control, 'p' })
        {
            m_completer.select(-1);
            update_completions = false;
        }

        if (update_completions)
            m_idle_timer.set_next_date(Clock::now() + idle_timeout);
        if (moved)
            context().hooks().run_hook("InsertMove", key_to_str(key), context());
    }
private:
    bool m_insert_reg = false;
    IncrementalInserter m_inserter;
    BufferCompleter     m_completer;
    Timer               m_idle_timer;
};

}

void InputMode::reset_normal_mode()
{
    m_input_handler.m_mode_trash.emplace_back(std::move(m_input_handler.m_mode));
    m_input_handler.m_mode.reset(new InputModes::Normal(m_input_handler));
}


InputHandler::InputHandler(UserInterface& ui)
    : m_context(*this, ui), m_mode(new InputModes::Normal(*this))
{
}

InputHandler::~InputHandler()
{
}

void InputHandler::insert(InsertMode mode)
{
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Insert(*this, mode));
}

void InputHandler::repeat_last_insert()
{
    if (m_last_insert.second.empty())
        return;

    std::vector<Key> keys;
    swap(keys, m_last_insert.second);
    // context.last_insert will be refilled by the new Insert
    // this is very inefficient.
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Insert(*this, m_last_insert.first));
    for (auto& key : keys)
        m_mode->on_key(key);
    assert(dynamic_cast<InputModes::Normal*>(m_mode.get()) != nullptr);
}

void InputHandler::prompt(const String& prompt, Completer completer,
                          PromptCallback callback)
{
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Prompt(*this, prompt, completer, callback));
}

void InputHandler::menu(const memoryview<String>& choices,
                        MenuCallback callback)
{
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Menu(*this, choices, callback));
}

void InputHandler::on_next_key(KeyCallback callback)
{
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::NextKey(*this, callback));
}

bool is_valid(const Key& key)
{
    return key != Key::Invalid and key.key <= 0x10FFFF;
}

void InputHandler::handle_available_inputs()
{
    m_mode_trash.clear();
    while (m_context.ui().is_key_available())
    {
        Key key = m_context.ui().get_key();
        if (is_valid(key))
        {
            const bool was_recording = is_recording();

            m_mode->on_key(key);

            // do not record the key that made us enter or leave recording mode.
            if (was_recording and is_recording())
                m_recorded_keys += key_to_str(key);
        }
        m_mode_trash.clear();
    }
}

void InputHandler::start_recording(char reg)
{
    assert(m_recording_reg == 0);
    m_recorded_keys = "";
    m_recording_reg = reg;
}

bool InputHandler::is_recording() const
{
    return m_recording_reg != 0;
}

void InputHandler::stop_recording()
{
    assert(m_recording_reg != 0);
    RegisterManager::instance()[m_recording_reg] = memoryview<String>(m_recorded_keys);
    m_recording_reg = 0;
}

}
