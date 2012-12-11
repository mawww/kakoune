#include "input_handler.hh"

#include "context.hh"
#include "editor.hh"
#include "register_manager.hh"
#include "utf8.hh"

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

    virtual void on_key(const Key& key, Context& context) = 0;
protected:
    void reset_normal_mode();
private:
    InputHandler& m_input_handler;
};

namespace InputModes
{

class Normal : public InputMode
{
public:
    Normal(InputHandler& input_handler)
        : InputMode(input_handler)
    {
    }

    void on_key(const Key& key, Context& context) override
    {
        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            m_count = m_count * 10 + key.key - '0';
        else
        {
            auto it = keymap.find(key);
            if (it != keymap.end())
            {
                context.numeric_param() = m_count;
                it->second(context);
            }
            m_count = 0;
        }
    }

private:
    int m_count = 0;
};

String codepoint_to_str(Codepoint cp)
{
    std::string str;
    auto it = back_inserter(str);
    utf8::dump(it, cp);
    return String(str);
}

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
    Menu(Context& context, const memoryview<String>& choices,
         MenuCallback callback)
        : InputMode(context.input_handler()),
          m_callback(callback), m_choices(choices.begin(), choices.end()),
          m_selected(m_choices.begin())
    {
        DisplayCoord menu_pos{ context.window().dimensions().line, 0_char };
        context.ui().menu_show(choices, menu_pos, MenuStyle::Prompt);
    }

    void on_key(const Key& key, Context& context) override
    {
        auto match_filter = [this](const String& str) {
            return boost::regex_match(str.begin(), str.end(), m_filter);
        };

        if (key == Key(Key::Modifiers::Control, 'm'))
        {
            context.ui().menu_hide();
            context.ui().print_status("");
            reset_normal_mode();
            int selected = m_selected - m_choices.begin();
            m_callback(selected, context);
            return;
        }
        else if (key == Key::Escape or key == Key{ Key::Modifiers::Control, 'c' })
        {
            if (m_edit_filter)
            {
                m_edit_filter = false;
                m_filter = boost::regex(".*");
                m_filter_editor.reset("");
                context.ui().print_status("");
            }
            else
            {
                context.ui().menu_hide();
                reset_normal_mode();
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
            m_selected = it;
            context.ui().menu_select(m_selected - m_choices.begin());
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
            m_selected = it.base()-1;
            context.ui().menu_select(m_selected - m_choices.begin());
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
            m_selected = it;
            context.ui().menu_select(m_selected - m_choices.begin());
        }

        if (m_edit_filter)
            context.ui().print_status("/" + m_filter_editor.line(),
                                      m_filter_editor.cursor_pos() + 1);
   }

private:
    MenuCallback m_callback;

    using ChoiceList = std::vector<String>;
    const ChoiceList m_choices;
    ChoiceList::const_iterator m_selected;

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
    Prompt(Context& context, const String& prompt,
           Completer completer, PromptCallback callback)
        : InputMode(context.input_handler()), m_prompt(prompt),
          m_completer(completer), m_callback(callback)
    {
        m_history_it = ms_history[m_prompt].end();
        context.ui().print_status(m_prompt, m_prompt.char_length());
    }

    void on_key(const Key& key, Context& context) override
    {
        std::vector<String>& history = ms_history[m_prompt];
        const String& line = m_line_editor.line();

        if (m_insert_reg)
        {
            String reg = RegisterManager::instance()[key.key].values(context)[0];
            m_line_editor.insert(reg);
            m_insert_reg = false;
        }
        else if (key == Key{Key::Modifiers::Control, 'm'}) // enter
        {
            std::vector<String>::iterator it;
            while ((it = find(history, line)) != history.end())
                history.erase(it);

            history.push_back(line);
            context.ui().print_status("");
            context.ui().menu_hide();
            reset_normal_mode();
            // call callback after reset_normal_mode so that callback
            // may change the mode
            m_callback(line, PromptEvent::Validate, context);
            return;
        }
        else if (key == Key::Escape or key == Key { Key::Modifiers::Control, 'c' })
        {
            context.ui().print_status("");
            context.ui().menu_hide();
            reset_normal_mode();
            m_callback(line, PromptEvent::Abort, context);
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
                m_completions = m_completer(context, line,
                                            line.byte_count_to(m_line_editor.cursor_pos()));
                if (candidates.empty())
                    return;

                context.ui().menu_hide();
                DisplayCoord menu_pos{ context.window().dimensions().line, 0_char };
                context.ui().menu_show(candidates, menu_pos, MenuStyle::Prompt);

                bool use_common_prefix = context.options()["complete_prefix"].as_int();
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
            context.ui().menu_select(m_current_completion);

            m_line_editor.insert_from(line.char_count_to(m_completions.start),
                                      completion);

            // when we have only one completion candidate, make next tab complete
            // from the new content.
            if (candidates.size() == 1)
                m_current_completion = -1;
        }
        else
        {
            context.ui().menu_hide();
            m_current_completion = -1;
            m_line_editor.handle_key(key);
        }
        context.ui().print_status(m_prompt + line,
                                  m_prompt.char_length() + m_line_editor.cursor_pos());
        m_callback(line, PromptEvent::Change, context);
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

    void on_key(const Key& key, Context& context) override
    {
        reset_normal_mode();
        m_callback(key, context);
   }

private:
    KeyCallback m_callback;
};

class WordCompleter
{
public:
    void select(const Context& context, int offset)
    {
        if (not setup_ifn(context))
            return;

        context.buffer().erase(m_position, m_position + m_completions[m_current_completion].length());
        m_current_completion = (m_current_completion + offset) % m_completions.size();
        context.buffer().insert(m_position, m_completions[m_current_completion]);
        context.ui().menu_select(m_current_completion);
    }

    void reset(const Context& context)
    {
        m_position = BufferIterator();
        context.ui().menu_hide();
    }
private:
    bool setup_ifn(const Context& context)
    {
        if (not m_position.is_valid())
        {
            BufferIterator end = context.editor().selections().back().last();
            BufferIterator begin = end-1;
            while (not begin.is_begin() and is_word(*begin))
                --begin;
            if (not is_word(*begin))
                ++begin;

            String prefix = context.buffer().string(begin, end);
            m_completions = complete_word(context, prefix, begin);
            if (m_completions.empty())
                return false;

            m_position = begin;
            DisplayCoord menu_pos = context.window().display_position(m_position);
            context.ui().menu_show(m_completions, menu_pos, MenuStyle::Inline);

            m_completions.push_back(prefix);
            m_current_completion = m_completions.size() - 1;
        }
        return true;
    }

    CandidateList complete_word(const Context& context,
                                const String& prefix,
                                const BufferIterator& pos)
    {
        String ex = "\\<\\Q" + prefix + "\\E\\w+\\>";
        Regex re(ex.begin(), ex.end());
        Buffer& buffer = context.buffer();
        boost::regex_iterator<BufferIterator> it(buffer.begin(), buffer.end(), re);
        boost::regex_iterator<BufferIterator> end;

        CandidateList result;
        for (; it != end; ++it)
        {
            auto& match = (*it)[0];
            if (match.first <= pos and pos < match.second)
                continue;

            String content = buffer.string(match.first, match.second);
            if (not contains(result, content))
                result.emplace_back(std::move(content));
        }
        std::sort(result.begin(), result.end());
        return result;
    }


    BufferIterator m_position;
    CandidateList  m_completions;
    int            m_current_completion = -1;
};

class Insert : public InputMode
{
public:
    Insert(Context& context, InsertMode mode)
        : InputMode(context.input_handler()),
          m_inserter(context.editor(), mode)
    {
        context.last_insert().first = mode;
        context.last_insert().second.clear();
    }

    void on_key(const Key& key, Context& context) override
    {
        context.last_insert().second.push_back(key);
        if (m_insert_reg)
        {
            if (key.modifiers == Key::Modifiers::None)
                m_inserter.insert(RegisterManager::instance()[key.key].values(context));
            m_insert_reg = false;
            return;
        }
        bool reset_completer = true;
        if (key == Key::Escape or key == Key{ Key::Modifiers::Control, 'c' })
        {
            m_completer.reset(context);
            reset_normal_mode();
        }
        else if (key == Key::Backspace)
            m_inserter.erase();
        else if (key == Key::Left)
            m_inserter.move_cursors(-1_char);
        else if (key == Key::Right)
            m_inserter.move_cursors(1_char);
        else if (key == Key::Up)
            m_inserter.move_cursors(-1_line);
        else if (key == Key::Down)
            m_inserter.move_cursors(1_line);
        else if (key.modifiers == Key::Modifiers::None)
        {
            m_inserter.insert(codepoint_to_str(key.key));
            if (m_inserter.editor().selections().size() == 1 and
                is_word(key.key))
            {
                m_completer.reset(context);
                reset_completer = false;
                m_completer.select(context, 0);
            }
        }
        else if (key == Key{ Key::Modifiers::Control, 'r' })
            m_insert_reg = true;
        else if ( key == Key{ Key::Modifiers::Control, 'm' })
            m_inserter.insert(String() + '\n');
        else if ( key == Key{ Key::Modifiers::Control, 'i' })
            m_inserter.insert(String() + '\t');
        else if ( key == Key{ Key::Modifiers::Control, 'n' })
        {
            m_completer.select(context, 1);
            reset_completer = false;
        }
        else if ( key == Key{ Key::Modifiers::Control, 'p' })
        {
            m_completer.select(context, -1);
            reset_completer = false;
        }

        if (reset_completer)
            m_completer.reset(context);
    }
private:
    bool m_insert_reg = false;
    IncrementalInserter m_inserter;
    WordCompleter m_completer;
};

}

void InputMode::reset_normal_mode()
{
    m_input_handler.m_mode_trash.emplace_back(std::move(m_input_handler.m_mode));
    m_input_handler.m_mode.reset(new InputModes::Normal(m_input_handler));
}


InputHandler::InputHandler()
    : m_mode(new InputModes::Normal(*this))
{
}

InputHandler::~InputHandler()
{
}

void InputHandler::insert(Context& context, InsertMode mode)
{
    assert(&context.input_handler() == this);
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Insert(context, mode));
}

void InputHandler::repeat_last_insert(Context& context)
{
    assert(&context.input_handler() == this);
    Context::Insertion& last_insert = context.last_insert();
    if (last_insert.second.empty())
        return;

    std::vector<Key> keys;
    swap(keys, last_insert.second);
    // context.last_insert will be refilled by the new Insert
    // this is very inefficient.
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Insert(context, last_insert.first));
    for (auto& key : keys)
        m_mode->on_key(key, context);
    assert(dynamic_cast<InputModes::Normal*>(m_mode.get()) != nullptr);
}

void InputHandler::prompt(const String& prompt, Completer completer,
                    PromptCallback callback, Context& context)
{
    assert(&context.input_handler() == this);
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Prompt(context, prompt, completer, callback));
}

void InputHandler::menu(const memoryview<String>& choices,
                  MenuCallback callback, Context& context)
{
    assert(&context.input_handler() == this);
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new InputModes::Menu(context, choices, callback));
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

void InputHandler::handle_available_inputs(Context& context)
{
    m_mode_trash.clear();
    while (context.ui().is_key_available())
    {
        Key key = context.ui().get_key();
        if (is_valid(key))
            m_mode->on_key(key, context);
        m_mode_trash.clear();
    }
}

}
