#include "input_handler.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "insert_completer.hh"
#include "normal.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "unordered_map.hh"
#include "user_interface.hh"
#include "utf8.hh"
#include "window.hh"

namespace Kakoune
{

class InputMode
{
public:
    InputMode(InputHandler& input_handler) : m_input_handler(input_handler) {}
    virtual ~InputMode() {}
    InputMode(const InputMode&) = delete;
    InputMode& operator=(const InputMode&) = delete;

    virtual void on_key(Key key) = 0;
    virtual void on_enabled() {}
    virtual void on_disabled() {}
    Context& context() const { return m_input_handler.context(); }

    virtual DisplayLine mode_line() const = 0;

    virtual KeymapMode keymap_mode() const = 0;

    using Insertion = InputHandler::Insertion;
    Insertion& last_insert() { return m_input_handler.m_last_insert; }

protected:
    void reset_normal_mode();
private:
    InputHandler& m_input_handler;
};

namespace InputModes
{

static constexpr std::chrono::milliseconds idle_timeout{50};
static constexpr std::chrono::milliseconds fs_check_timeout{500};

struct MouseHandler
{
    bool handle_key(Key key, Context& context)
    {
        if (not context.has_window())
            return false;

        if (key.modifiers == Key::Modifiers::MousePress)
        {
            m_dragging = true;
            m_anchor = context.window().buffer_coord(key.mouse_coord());
            context.selections_write_only() = SelectionList{ context.buffer(), m_anchor };
            return true;
        }
        if (key.modifiers == Key::Modifiers::MouseRelease)
        {
            if (not m_dragging)
                return true;
            m_dragging = false;
            auto cursor = context.window().buffer_coord(key.mouse_coord());
            context.selections_write_only() = SelectionList{ context.buffer(), Selection{m_anchor, cursor} };
            return true;
        }
        if (key.modifiers == Key::Modifiers::MousePos)
        {
            if (not m_dragging)
                return true;
            auto cursor = context.window().buffer_coord(key.mouse_coord());
            context.selections_write_only() = SelectionList{ context.buffer(), Selection{m_anchor, cursor} };
            return true;
        }
        if (key.modifiers == Key::Modifiers::MouseWheelDown)
        {
            wheel(context, 3);
            return true;
        }
        if (key.modifiers == Key::Modifiers::MouseWheelUp)
        {
            wheel(context, -3);
            return true;
        }
        return false;
    }

private:
    void wheel(Context& context, LineCount offset)
    {
        Window& window = context.window();
        Buffer& buffer = context.buffer();

        CharCoord win_pos = window.position();
        CharCoord win_dim = window.dimensions();

        const CharCoord max_offset{(win_dim.line - 1)/2, (win_dim.column - 1)/2};
        const CharCoord scrolloff =
            std::min(context.options()["scrolloff"].get<CharCoord>(), max_offset);

        const LineCount line_count = buffer.line_count();
        win_pos.line = clamp(win_pos.line + offset, 0_line, line_count-1);

        SelectionList& selections = context.selections();
        const ByteCoord cursor = selections.main().cursor();

        auto clamp_line = [&](LineCount line) { return clamp(line, 0_line, line_count-1); };
        auto min_coord = buffer.offset_coord(clamp_line(win_pos.line + scrolloff.line), win_pos.column);
        auto max_coord = buffer.offset_coord(clamp_line(win_pos.line + win_dim.line - scrolloff.line), win_pos.column);

        selections = SelectionList{buffer, clamp(cursor, min_coord, max_coord)};

        window.set_position(win_pos);
    }

    bool m_dragging = false;
    ByteCoord m_anchor;
};

class Normal : public InputMode
{
public:
    Normal(InputHandler& input_handler)
        : InputMode(input_handler),
          m_idle_timer{Clock::now() + idle_timeout,
                       context().flags() & Context::Flags::Transient ?
                           Timer::Callback() : Timer::Callback([this](Timer& timer) {
              context().hooks().run_hook("NormalIdle", "", context());
          })},
          m_fs_check_timer{Clock::now() + fs_check_timeout,
                           context().flags() & Context::Flags::Transient ?
                            Timer::Callback() : Timer::Callback([this](Timer& timer) {
              if (not context().has_client())
                  return;
              context().client().check_if_buffer_needs_reloading();
              timer.set_next_date(Clock::now() + fs_check_timeout);
          })}
    {}

    void on_enabled() override
    {
        if (context().has_client())
        {
            context().client().check_if_buffer_needs_reloading();
            m_fs_check_timer.set_next_date(Clock::now() + fs_check_timeout);
        }

        context().hooks().run_hook("NormalBegin", "", context());
    }

    void on_disabled() override
    {
        context().hooks().run_hook("NormalEnd", "", context());
    }

    void on_key(Key key) override
    {
        if (m_mouse_handler.handle_key(key, context()))
            return;

        if (m_waiting_for_reg)
        {
            if (key.modifiers == Key::Modifiers::None)
                m_params.reg = key.key;
            m_waiting_for_reg = false;
            return;
        }
        bool do_restore_hooks = false;
        auto restore_hooks = on_scope_end([&, this]{
            if (do_restore_hooks)
            {
                context().user_hooks_support().enable();
                m_hooks_disabled = false;
            }
        });

        context().print_status({});
        if (context().has_ui())
            context().ui().info_hide();

        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            m_params.count = m_params.count * 10 + key.key - '0';
        else if (key == Key::Backspace)
            m_params.count /= 10;
        else if (key == '\\')
        {
            if (not m_hooks_disabled)
            {
                m_hooks_disabled = true;
                context().user_hooks_support().disable();
            }
        }
        else if (key == '"')
        {
            m_waiting_for_reg = true;
        }
        else
        {
            if (m_hooks_disabled)
                do_restore_hooks = true;
            auto it = std::lower_bound(keymap.begin(), keymap.end(), key,
                                       [](const NormalCmdDesc& lhs, const Key& rhs)
                                       { return lhs.key < rhs; });
            if (it != keymap.end() and it->key == key)
            {
                if (context().options()["autoinfo"].get<int>() >= 2 and context().has_ui())
                    context().ui().info_show(key_to_str(key), it->docstring, CharCoord{},
                                             get_face("Information"), InfoStyle::Prompt);
                it->func(context(), m_params);
            }
            m_params = { 0, '"' };
        }

        context().hooks().run_hook("NormalKey", key_to_str(key), context());
        m_idle_timer.set_next_date(Clock::now() + idle_timeout);
    }

    DisplayLine mode_line() const override
    {
        AtomList atoms = { { to_string(context().selections().size()) + " sel", Face(Color::Blue) } };
        if (m_params.count != 0)
        {
            atoms.push_back({ "; param=", Face(Color::Yellow) });
            atoms.push_back({ to_string(m_params.count), Face(Color::Green) });
        }
        if (m_params.reg != '"')
        {
            atoms.push_back({ "; reg=", Face(Color::Yellow) });
            atoms.push_back({ StringView(m_params.reg).str(), Face(Color::Green) });
        }
        return atoms;
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Normal; }

private:
    NormalParams m_params = { 0, '"' };
    bool m_hooks_disabled = false;
    bool m_waiting_for_reg = false;
    Timer m_idle_timer;
    Timer m_fs_check_timer;
    MouseHandler m_mouse_handler;
};

template<WordType word_type>
void to_next_word_begin(CharCount& pos, StringView line)
{
    const CharCount len = line.char_length();
    if (pos == len)
        return;
    if (word_type == Word and is_punctuation(line[pos]))
    {
        while (pos != len and is_punctuation(line[pos]))
            ++pos;
    }
    else if (is_word<word_type>(line[pos]))
    {
        while (pos != len and is_word<word_type>(line[pos]))
            ++pos;
    }
    while (pos != len and is_horizontal_blank(line[pos]))
        ++pos;
}

template<WordType word_type>
void to_next_word_end(CharCount& pos, StringView line)
{
    const CharCount len = line.char_length();
    if (pos + 1 >= len)
        return;
    ++pos;

    while (pos != len and is_horizontal_blank(line[pos]))
        ++pos;

    if (word_type == Word and is_punctuation(line[pos]))
    {
        while (pos != len and is_punctuation(line[pos]))
            ++pos;
    }
    else if (is_word<word_type>(line[pos]))
    {
        while (pos != len and is_word<word_type>(line[pos]))
            ++pos;
    }
    --pos;
}

template<WordType word_type>
void to_prev_word_begin(CharCount& pos, StringView line)
{
    if (pos == 0_char)
        return;
    --pos;

    while (pos != 0_char and is_horizontal_blank(line[pos]))
        --pos;

    if (word_type == Word and is_punctuation(line[pos]))
    {
        while (pos != 0_char and is_punctuation(line[pos]))
            --pos;
        if (!is_punctuation(line[pos]))
            ++pos;
    }
    else if (is_word<word_type>(line[pos]))
    {
        while (pos != 0_char and is_word<word_type>(line[pos]))
            --pos;
        if (!is_word<word_type>(line[pos]))
            ++pos;
     }
}

class LineEditor
{
public:
    void handle_key(Key key)
    {
        if (key == Key::Left or key == alt('h'))
        {
            if (m_cursor_pos > 0)
                --m_cursor_pos;
        }
        else if (key == Key::Right or key == alt('l'))
        {
            if (m_cursor_pos < m_line.char_length())
                ++m_cursor_pos;
        }
        else if (key == Key::Home)
            m_cursor_pos = 0;
        else if (key == Key::End)
            m_cursor_pos = m_line.char_length();
        else if (key == Key::Backspace or key == alt('x'))
        {
            if (m_cursor_pos != 0)
            {
                m_line = m_line.substr(0, m_cursor_pos - 1)
                       + m_line.substr(m_cursor_pos);

                --m_cursor_pos;
            }
        }
        else if (key == Key::Delete or key == alt('d'))
        {
            if (m_cursor_pos != m_line.char_length())
                m_line = m_line.substr(0, m_cursor_pos)
                       + m_line.substr(m_cursor_pos+1);
        }
        else if (key == ctrl('w'))
            to_next_word_begin<Word>(m_cursor_pos, m_line);
        else if (key == ctrlalt('w'))
            to_next_word_begin<WORD>(m_cursor_pos, m_line);
        else if (key == ctrl('b'))
            to_prev_word_begin<Word>(m_cursor_pos, m_line);
        else if (key == ctrlalt('b'))
            to_prev_word_begin<WORD>(m_cursor_pos, m_line);
        else if (key == ctrl('e'))
            to_next_word_end<Word>(m_cursor_pos, m_line);
        else if (key == ctrlalt('e'))
            to_next_word_end<WORD>(m_cursor_pos, m_line);
        else if (key.modifiers == Key::Modifiers::None)
        {
            m_line = m_line.substr(0, m_cursor_pos) + codepoint_to_str(key.key)
                   + m_line.substr(m_cursor_pos);
            ++m_cursor_pos;
        }
    }

    void insert(StringView str)
    {
        insert_from(m_cursor_pos, str);
    }

    void insert_from(CharCount start, StringView str)
    {
        kak_assert(start <= m_cursor_pos);
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

    DisplayLine build_display_line(CharCount width)
    {
        kak_assert(m_cursor_pos <= m_line.char_length());
        if (m_cursor_pos < m_display_pos)
            m_display_pos = m_cursor_pos;
        if (m_cursor_pos >= m_display_pos + width)
            m_display_pos = m_cursor_pos + 1 - width;

        if (m_cursor_pos == m_line.char_length())
            return DisplayLine{{ {m_line.substr(m_display_pos, width-1).str(), get_face("StatusLine")},
                                 {" "_str, get_face("StatusCursor")} }};
        else
            return DisplayLine({ { m_line.substr(m_display_pos, m_cursor_pos - m_display_pos).str(), get_face("StatusLine") },
                                 { m_line.substr(m_cursor_pos,1).str(), get_face("StatusCursor") },
                                 { m_line.substr(m_cursor_pos+1, width - m_cursor_pos + m_display_pos - 1).str(), get_face("StatusLine") } });
    }
private:
    CharCount      m_cursor_pos = 0;
    CharCount      m_display_pos = 0;

    String         m_line;
};

class Menu : public InputMode
{
public:
    Menu(InputHandler& input_handler, ConstArrayView<String> choices,
         MenuCallback callback)
        : InputMode(input_handler),
          m_callback(callback), m_choices(choices.begin(), choices.end()),
          m_selected(m_choices.begin())
    {
        if (not context().has_ui())
            return;
        context().ui().menu_show(choices, CharCoord{}, get_face("MenuForeground"),
                                 get_face("MenuBackground"), MenuStyle::Prompt);
        context().ui().menu_select(0);
    }

    void on_key(Key key) override
    {
        auto match_filter = [this](const String& str) {
            return regex_match(str.begin(), str.end(), m_filter);
        };

        if (key == ctrl('m'))
        {
            if (context().has_ui())
                context().ui().menu_hide();
            context().print_status(DisplayLine{});
            reset_normal_mode();
            int selected = m_selected - m_choices.begin();
            m_callback(selected, MenuEvent::Validate, context());
            return;
        }
        else if (key == Key::Escape or key == ctrl('c'))
        {
            if (m_edit_filter)
            {
                m_edit_filter = false;
                m_filter = Regex(".*");
                m_filter_editor.reset("");
                context().print_status(DisplayLine{});
            }
            else
            {
                if (context().has_ui())
                    context().ui().menu_hide();
                reset_normal_mode();
                int selected = m_selected - m_choices.begin();
                m_callback(selected, MenuEvent::Abort, context());
            }
        }
        else if (key == Key::Down or key == ctrl('i') or
                 key == ctrl('n') or (not m_edit_filter and key == 'j'))
        {
            auto it = std::find_if(m_selected+1, m_choices.end(), match_filter);
            if (it == m_choices.end())
                it = std::find_if(m_choices.begin(), m_selected, match_filter);
            select(it);
        }
        else if (key == Key::Up or key == Key::BackTab or
                 key == ctrl('p') or (not m_edit_filter and key == 'k'))
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
            m_filter = Regex(search.begin(), search.end());
            auto it = std::find_if(m_selected, m_choices.end(), match_filter);
            if (it == m_choices.end())
                it = std::find_if(m_choices.begin(), m_selected, match_filter);
            select(it);
        }

        if (m_edit_filter and context().has_ui())
        {
            auto prompt = "filter:"_str;
            auto width = context().ui().dimensions().column - prompt.char_length();
            auto display_line = m_filter_editor.build_display_line(width);
            display_line.insert(display_line.begin(), { prompt, get_face("Prompt") });
            context().print_status(display_line);
        }
    }

    DisplayLine mode_line() const override
    {
        return { "menu", Face(Color::Yellow) };
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Menu; }

private:
    MenuCallback m_callback;

    using ChoiceList = Vector<String>;
    const ChoiceList m_choices;
    ChoiceList::const_iterator m_selected;

    void select(ChoiceList::const_iterator it)
    {
        m_selected = it;
        int selected = m_selected - m_choices.begin();
        if (context().has_ui())
            context().ui().menu_select(selected);
        m_callback(selected, MenuEvent::Select, context());
    }

    Regex      m_filter = Regex(".*");
    bool       m_edit_filter = false;
    LineEditor m_filter_editor;
};

String common_prefix(ConstArrayView<String> strings)
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
            res = res.substr(0, common_len).str();
    }
    return res;
}

class Prompt : public InputMode
{
public:
    Prompt(InputHandler& input_handler, StringView prompt,
           String initstr, Face face, Completer completer,
           PromptCallback callback)
        : InputMode(input_handler), m_prompt(prompt.str()), m_prompt_face(face),
          m_completer(completer), m_callback(callback),
          m_autoshowcompl{context().options()["autoshowcompl"].get<bool>()}
    {
        m_history_it = ms_history[m_prompt].end();
        if (m_autoshowcompl)
            refresh_completions(CompletionFlags::Fast);
        m_line_editor.reset(std::move(initstr));
        display();
    }

    void on_key(Key key) override
    {
        History& history = ms_history[m_prompt];
        const String& line = m_line_editor.line();
        bool showcompl = false;

        if (m_mode == Mode::InsertReg)
        {
            StringView reg = context().main_sel_register_value(String{key.key});
            m_line_editor.insert(reg);
            m_mode = Mode::Default;
        }
        else if (key == ctrl('m')) // enter
        {
            if (context().history_support().is_enabled())
                history_push(history, line);
            context().print_status(DisplayLine{});
            if (context().has_ui())
                context().ui().menu_hide();
            reset_normal_mode();
            // call callback after reset_normal_mode so that callback
            // may change the mode
            m_callback(line, PromptEvent::Validate, context());
            return;
        }
        else if (key == Key::Escape or key == ctrl('c'))
        {
            if (context().history_support().is_enabled())
                history_push(history, line);
            context().print_status(DisplayLine{});
            if (context().has_ui())
                context().ui().menu_hide();
            reset_normal_mode();
            m_callback(line, PromptEvent::Abort, context());
            return;
        }
        else if (key == ctrl('r'))
        {
            m_mode = Mode::InsertReg;
        }
        else if (key == Key::Up or key == ctrl('p'))
        {
            if (m_history_it != history.begin())
            {
                if (m_history_it == history.end())
                   m_prefix = line;
                auto it = m_history_it;
                // search for the previous history entry matching typed prefix
                do
                {
                    --it;
                    if (prefix_match(*it, m_prefix))
                    {
                        m_history_it = it;
                        m_line_editor.reset(*it);
                        break;
                    }
                } while (it != history.begin());

                clear_completions();
                showcompl = true;
            }
        }
        else if (key == Key::Down or key == ctrl('n')) // next
        {
            if (m_history_it != history.end())
            {
                // search for the next history entry matching typed prefix
                ++m_history_it;
                while (m_history_it != history.end() and
                       prefix_match(*m_history_it, m_prefix))
                    ++m_history_it;

                if (m_history_it != history.end())
                    m_line_editor.reset(*m_history_it);
                else
                    m_line_editor.reset(m_prefix);

                clear_completions();
                showcompl = true;
            }
        }
        else if (key == ctrl('i') or key == Key::BackTab) // tab completion
        {
            const bool reverse = (key == Key::BackTab);
            CandidateList& candidates = m_completions.candidates;
            // first try, we need to ask our completer for completions
            bool updated_completions = false;
            if (candidates.empty())
            {
                refresh_completions(CompletionFlags::None);

                if (candidates.empty())
                    return;
                updated_completions = true;
            }
            bool did_prefix = false;
            if (m_current_completion == -1 and
                context().options()["complete_prefix"].get<bool>())
            {
                const String& line = m_line_editor.line();
                CandidateList& candidates = m_completions.candidates;
                String prefix = common_prefix(candidates);
                if (m_completions.end - m_completions.start > prefix.length())
                    prefix = line.substr(m_completions.start,
                                         m_completions.end - m_completions.start).str();

                if (not prefix.empty())
                {
                    auto it = find(candidates, prefix);
                    if (it == candidates.end())
                    {
                        m_current_completion = candidates.size();
                        candidates.push_back(prefix);
                    }
                    else
                        m_current_completion = it - candidates.begin();

                    CharCount start = line.char_count_to(m_completions.start);
                    // When we just updated completions, select the common
                    // prefix even if it was the currently entered text.
                    did_prefix = updated_completions or
                                 prefix != line.substr(start, m_line_editor.cursor_pos() - start);
                }
            }
            if (not did_prefix)
            {
                if (not reverse and ++m_current_completion >= candidates.size())
                    m_current_completion = 0;
                else if (reverse and --m_current_completion < 0)
                    m_current_completion = candidates.size()-1;
            }

            const String& completion = candidates[m_current_completion];
            if (context().has_ui())
                context().ui().menu_select(m_current_completion);

            m_line_editor.insert_from(line.char_count_to(m_completions.start),
                                      completion);

            // when we have only one completion candidate, make next tab complete
            // from the new content.
            if (candidates.size() == 1)
            {
                m_current_completion = -1;
                candidates.clear();
                showcompl = true;
            }
        }
        else if (key == ctrl('o'))
        {
            m_autoshowcompl = false;
            clear_completions();
        }
        else
        {
            m_line_editor.handle_key(key);
            clear_completions();
            showcompl = true;
        }

        if (showcompl and m_autoshowcompl)
            refresh_completions(CompletionFlags::Fast);

        display();
        m_callback(line, PromptEvent::Change, context());
    }

    void set_prompt_face(Face face)
    {
        if (face != m_prompt_face)
        {
            m_prompt_face = face;
            display();
        }
    }

    DisplayLine mode_line() const override
    {
        return { "prompt", Face(Color::Yellow) };
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Prompt; }

private:
    void refresh_completions(CompletionFlags flags)
    {
        try
        {
            if (not m_completer)
                return;
            m_current_completion = -1;
            const String& line = m_line_editor.line();
            m_completions = m_completer(context(), flags, line,
                                        line.byte_count_to(m_line_editor.cursor_pos()));
            CandidateList& candidates = m_completions.candidates;
            if (context().has_ui() and not candidates.empty())
                context().ui().menu_show(candidates, CharCoord{}, get_face("MenuForeground"),
                                         get_face("MenuBackground"), MenuStyle::Prompt);
        } catch (runtime_error&) {}
    }

    void clear_completions()
    {
        m_current_completion = -1;
        m_completions.candidates.clear();
        if (context().has_ui())
            context().ui().menu_hide();
    }

    void display()
    {
        if (not context().has_ui())
            return;

        auto width = context().ui().dimensions().column - m_prompt.char_length();
        auto display_line = m_line_editor.build_display_line(width);
        display_line.insert(display_line.begin(), { m_prompt, m_prompt_face });
        context().print_status(display_line);
    }

    enum class Mode { Default, InsertReg };

    PromptCallback m_callback;
    Completer      m_completer;
    const String   m_prompt;
    Face           m_prompt_face;
    Completions    m_completions;
    int            m_current_completion = -1;
    String         m_prefix;
    LineEditor     m_line_editor;
    bool           m_autoshowcompl;
    Mode           m_mode = Mode::Default;

    using History = Vector<String, MemoryDomain::History>;
    static UnorderedMap<String, History, MemoryDomain::History> ms_history;
    History::iterator m_history_it;

    static void history_push(History& history, StringView entry)
    {
        if(entry.empty())
        {
            return;
        }
        History::iterator it;
        while ((it = find(history, entry)) != history.end())
            history.erase(it);
        history.push_back(entry.str());
    }
};
UnorderedMap<String, Prompt::History, MemoryDomain::History> Prompt::ms_history;

class NextKey : public InputMode
{
public:
    NextKey(InputHandler& input_handler, KeymapMode keymap_mode, KeyCallback callback)
        : InputMode(input_handler), m_keymap_mode(keymap_mode), m_callback(callback) {}

    void on_key(Key key) override
    {
        reset_normal_mode();
        m_callback(key, context());
    }

    DisplayLine mode_line() const override
    {
        return { "enter key", Face(Color::Yellow) };
    }

    KeymapMode keymap_mode() const override { return m_keymap_mode; }

private:
    KeyCallback m_callback;
    KeymapMode m_keymap_mode;
};

class Insert : public InputMode
{
public:
    Insert(InputHandler& input_handler, InsertMode mode)
        : InputMode(input_handler),
          m_insert_mode(mode),
          m_edition(context()),
          m_completer(context()),
          m_autoshowcompl(true),
          m_idle_timer{Clock::now() + idle_timeout,
                       [this](Timer& timer) {
                           context().hooks().run_hook("InsertIdle", "", context());
                           if (m_autoshowcompl)
                               m_completer.update();
                       }},
          m_disable_hooks{context().user_hooks_support().is_disabled()}
    {
        // Prolongate hook disabling for the whole insert session
        if (m_disable_hooks)
            context().user_hooks_support().disable();

        last_insert().first = mode;
        last_insert().second.clear();
        context().hooks().run_hook("InsertBegin", "", context());
        prepare(m_insert_mode);
    }

    void on_key(Key key) override
    {
        auto& buffer = context().buffer();
        last_insert().second.push_back(key);
        if (m_mode == Mode::InsertReg)
        {
            if (key.modifiers == Key::Modifiers::None)
                insert(RegisterManager::instance()[key.key].values(context()));
            m_mode = Mode::Default;
            return;
        }
        if (m_mode == Mode::Complete)
        {
            if (key.key == 'f')
                m_completer.explicit_file_complete();
            if (key.key == 'w')
                m_completer.explicit_word_complete();
            if (key.key == 'l')
                m_completer.explicit_line_complete();
            m_mode = Mode::Default;
            return;
        }

        bool update_completions = true;
        bool moved = false;
        if (key == Key::Escape or key == ctrl('c'))
        {
            context().hooks().run_hook("InsertEnd", "", context());
            m_completer.reset();
            reset_normal_mode();
        }
        else if (key == Key::Backspace)
        {
            Vector<Selection> sels;
            for (auto& sel : context().selections())
            {
                if (sel.cursor() == ByteCoord{0,0})
                    continue;
                auto pos = sel.cursor();
                sels.push_back({ buffer.char_prev(pos) });
            }
            if (not sels.empty())
                SelectionList{buffer, std::move(sels)}.erase();
        }
        else if (key == Key::Delete)
        {
            Vector<Selection> sels;
            for (auto& sel : context().selections())
                sels.push_back({ sel.cursor() });
            SelectionList{buffer, std::move(sels)}.erase();
        }
        else if (key == Key::Left)
        {
            move(-1_char);
            moved = true;
        }
        else if (key == Key::Right)
        {
            move(1_char);
            moved = true;
        }
        else if (key == Key::Up)
        {
            move(-1_line);
            moved = true;
        }
        else if (key == Key::Down)
        {
            move(1_line);
            moved = true;
        }
        else if (key == Key::Home)
        {
            auto& selections = context().selections();
            for (auto& sel : selections)
                sel.anchor() = sel.cursor() = ByteCoord{sel.cursor().line, 0};
            selections.sort_and_merge_overlapping();
        }
        else if (key == Key::End)
        {
            auto& buffer = context().buffer();
            auto& selections = context().selections();
            for (auto& sel : selections)
            {
                const LineCount line = sel.cursor().line;
                sel.anchor() = sel.cursor() = buffer.clamp({line, buffer[line].length()});
            }
            selections.sort_and_merge_overlapping();
        }
        else if (key.modifiers == Key::Modifiers::None)
            insert(key.key);
        else if (key == ctrl('r'))
            m_mode = Mode::InsertReg;
        else if (key == ctrl('m'))
            insert('\n');
        else if (key == ctrl('i'))
            insert('\t');
        else if (key == ctrl('n'))
        {
            last_insert().second.pop_back();
            m_completer.select(1, last_insert().second);
            update_completions = false;
        }
        else if (key == ctrl('p'))
        {
            last_insert().second.pop_back();
            m_completer.select(-1, last_insert().second);
            update_completions = false;
        }
        else if (key == ctrl('x'))
            m_mode = Mode::Complete;
        else if (key == ctrl('o'))
        {
            m_autoshowcompl = false;
            m_completer.reset();
        }
        else if (key == ctrl('u'))
            context().buffer().commit_undo_group();

        context().hooks().run_hook("InsertKey", key_to_str(key), context());

        if (update_completions)
            m_idle_timer.set_next_date(Clock::now() + idle_timeout);
        if (moved)
            context().hooks().run_hook("InsertMove", key_to_str(key), context());
    }

    DisplayLine mode_line() const override
    {
        auto num_sel = context().selections().size();
        return {AtomList{ { "insert ", Face(Color::Green) },
                          { to_string(num_sel) + " sel", Face(Color::Blue) } }};
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Insert; }

private:
    template<typename Type>
    void move(Type offset)
    {
        auto& selections = context().selections();
        for (auto& sel : selections)
        {
            auto cursor = context().has_window() ? context().window().offset_coord(sel.cursor(), offset)
                                                 : context().buffer().offset_coord(sel.cursor(), offset);
            sel.anchor() = sel.cursor() = cursor;
        }
        selections.sort_and_merge_overlapping();
    }

    void insert(ConstArrayView<String> strings)
    {
        context().selections().insert(strings, InsertMode::InsertCursor);
    }

    void insert(Codepoint key)
    {
        auto str = codepoint_to_str(key);
        context().selections().insert(str, InsertMode::InsertCursor);
        context().hooks().run_hook("InsertChar", str, context());
    }

    void prepare(InsertMode mode)
    {
        SelectionList& selections = context().selections();
        Buffer& buffer = context().buffer();

        switch (mode)
        {
        case InsertMode::Insert:
            for (auto& sel : selections)
                sel = Selection{sel.max(), sel.min()};
            break;
        case InsertMode::Replace:
            selections.erase();
            break;
        case InsertMode::Append:
            for (auto& sel : selections)
            {
                sel = Selection{sel.min(), sel.max()};
                auto& cursor = sel.cursor();
                // special case for end of lines, append to current line instead
                if (cursor.column != buffer[cursor.line].length() - 1)
                    cursor = buffer.char_next(cursor);
            }
            break;
        case InsertMode::AppendAtLineEnd:
            for (auto& sel : selections)
                sel = ByteCoord{sel.max().line, buffer[sel.max().line].length() - 1};
            break;
        case InsertMode::OpenLineBelow:
            for (auto& sel : selections)
                sel = ByteCoord{sel.max().line, buffer[sel.max().line].length() - 1};
            insert('\n');
            break;
        case InsertMode::OpenLineAbove:
            for (auto& sel : selections)
            {
                auto line = sel.min().line;
                sel = line > 0 ? ByteCoord{line - 1, buffer[line-1].length() - 1}
                               : ByteCoord{0, 0};
            }
            insert('\n');
            // fix case where we inserted at begining
            for (auto& sel : selections)
            {
                if (sel.anchor() == buffer.char_next({0,0}))
                    sel = Selection{{0,0}};
            }
            break;
        case InsertMode::InsertAtLineBegin:
            for (auto& sel : selections)
            {
                ByteCoord pos = sel.min().line;
                auto pos_non_blank = buffer.iterator_at(pos);
                while (*pos_non_blank == ' ' or *pos_non_blank == '\t')
                    ++pos_non_blank;
                if (*pos_non_blank != '\n')
                    pos = pos_non_blank.coord();
                sel = pos;
            }
            break;
        case InsertMode::InsertAtNextLineBegin:
        case InsertMode::InsertCursor:
             kak_assert(false); // invalid for interactive insert
             break;
        }
        if (mode != InsertMode::Append and mode != InsertMode::Replace)
            selections.sort_and_merge_overlapping();
        selections.check_invariant();
        buffer.check_invariant();
    }

    void on_disabled() override
    {
        auto& selections = context().selections();
        for (auto& sel : selections)
        {
            if (m_insert_mode == InsertMode::Append and sel.cursor().column > 0)
                sel.cursor() = context().buffer().char_prev(sel.cursor());
        }
        selections.avoid_eol();

        if (m_disable_hooks)
            context().user_hooks_support().enable();
    }

    enum class Mode { Default, Complete, InsertReg };
    Mode m_mode = Mode::Default;
    InsertMode       m_insert_mode;
    ScopedEdition    m_edition;
    InsertCompleter  m_completer;
    bool             m_autoshowcompl;
    Timer            m_idle_timer;
    bool             m_disable_hooks;
};

}

void InputMode::reset_normal_mode()
{
    m_input_handler.reset_normal_mode();
}

InputHandler::InputHandler(SelectionList selections, Context::Flags flags, String name)
    : m_context(*this, std::move(selections), flags, std::move(name)),
      m_mode(new InputModes::Normal(*this))
{}

InputHandler::~InputHandler()
{}

void InputHandler::change_input_mode(InputMode* new_mode)
{
    m_mode->on_disabled();
    m_mode_trash.emplace_back(std::move(m_mode));
    m_mode.reset(new_mode);
    new_mode->on_enabled();
}

void InputHandler::insert(InsertMode mode)
{
    change_input_mode(new InputModes::Insert(*this, mode));
}

void InputHandler::repeat_last_insert()
{
    if (m_last_insert.second.empty())
        return;

    Vector<Key> keys;
    swap(keys, m_last_insert.second);
    // context.last_insert will be refilled by the new Insert
    // this is very inefficient.
    change_input_mode(new InputModes::Insert(*this, m_last_insert.first));
    for (auto& key : keys)
        m_mode->on_key(key);
    kak_assert(dynamic_cast<InputModes::Normal*>(m_mode.get()) != nullptr);
}

void InputHandler::prompt(StringView prompt, String initstr,
                          Face prompt_face, Completer completer,
                          PromptCallback callback)
{
    change_input_mode(new InputModes::Prompt(*this, prompt, initstr,
                                             prompt_face, completer,
                                             callback));
}

void InputHandler::set_prompt_face(Face prompt_face)
{
    InputModes::Prompt* prompt = dynamic_cast<InputModes::Prompt*>(m_mode.get());
    if (prompt)
        prompt->set_prompt_face(prompt_face);
}

void InputHandler::menu(ConstArrayView<String> choices,
                       MenuCallback callback)
{
    change_input_mode(new InputModes::Menu(*this, choices, callback));
}

void InputHandler::on_next_key(KeymapMode keymap_mode, KeyCallback callback)
{
    change_input_mode(new InputModes::NextKey(*this, keymap_mode, callback));
}

static bool is_valid(Key key)
{
    return key != Key::Invalid and
        ((key.modifiers & ~Key::Modifiers::ControlAlt) or key.key <= 0x10FFFF);
}

void InputHandler::handle_key(Key key)
{
    if (is_valid(key))
    {
        const bool was_recording = is_recording();
        ++m_handle_key_level;
        auto dec = on_scope_end([&]{ --m_handle_key_level; });

        auto keymap_mode = m_mode->keymap_mode();
        KeymapManager& keymaps = m_context.keymaps();
        if (keymaps.is_mapped(key, keymap_mode) and
            m_context.keymaps_support().is_enabled())
        {
            for (auto& k : keymaps.get_mapping(key, keymap_mode))
                m_mode->on_key(k);
        }
        else
            m_mode->on_key(key);

        // do not record the key that made us enter or leave recording mode,
        // and the ones that are triggered recursively by previous keys.
        if (was_recording and is_recording() and m_handle_key_level == 1)
            m_recorded_keys += key_to_str(key);
    }
}

void InputHandler::start_recording(char reg)
{
    kak_assert(m_recording_reg == 0);
    m_recorded_keys = "";
    m_recording_reg = reg;
}

bool InputHandler::is_recording() const
{
    return m_recording_reg != 0;
}

void InputHandler::stop_recording()
{
    kak_assert(m_recording_reg != 0);
    RegisterManager::instance()[m_recording_reg] = ConstArrayView<String>(m_recorded_keys);
    m_recording_reg = 0;
}

void InputHandler::reset_normal_mode()
{
    change_input_mode(new InputModes::Normal(*this));
}

DisplayLine InputHandler::mode_line() const
{
    return m_mode->mode_line();
}

void InputHandler::clear_mode_trash()
{
    m_mode_trash.clear();
}

}
