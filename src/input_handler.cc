#include <utility>

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
#include "hash_map.hh"
#include "user_interface.hh"
#include "utf8.hh"
#include "window.hh"

namespace Kakoune
{

class InputMode : public RefCountable
{
public:
    InputMode(InputHandler& input_handler) : m_input_handler(input_handler) {}
    ~InputMode() override = default;
    InputMode(const InputMode&) = delete;
    InputMode& operator=(const InputMode&) = delete;

    void handle_key(Key key) { RefPtr<InputMode> keep_alive{this}; on_key(key); }

    virtual void on_enabled() {}
    virtual void on_disabled(bool temporary) {}

    bool enabled() const { return &m_input_handler.current_mode() == this; }
    Context& context() const { return m_input_handler.context(); }

    virtual DisplayLine mode_line() const = 0;

    virtual KeymapMode keymap_mode() const = 0;

    virtual StringView name() const = 0;

    virtual std::pair<CursorMode, DisplayCoord> get_cursor_info() const
    {
        const auto cursor = context().selections().main().cursor();
        auto coord = context().window().display_position(cursor).value_or(DisplayCoord{});
        return {CursorMode::Buffer, coord};
    }

    using Insertion = InputHandler::Insertion;
    Insertion& last_insert() { return m_input_handler.m_last_insert; }

protected:
    virtual void on_key(Key key) = 0;

    void push_mode(InputMode* new_mode)
    {
        m_input_handler.push_mode(new_mode);
    }

    void pop_mode()
    {
        m_input_handler.pop_mode(this);
    }
private:
    InputHandler& m_input_handler;
};

namespace InputModes
{

std::chrono::milliseconds get_idle_timeout(const Context& context)
{
    return std::chrono::milliseconds{context.options()["idle_timeout"].get<int>()};
}

std::chrono::milliseconds get_fs_check_timeout(const Context& context)
{
    return std::chrono::milliseconds{context.options()["fs_check_timeout"].get<int>()};
}

struct MouseHandler
{
    bool handle_key(Key key, Context& context)
    {
        if (not context.has_window())
            return false;

        Buffer& buffer = context.buffer();
        BufferCoord cursor;
        auto& selections = context.selections();
        switch ((Key::Modifiers)(key.modifiers & Key::Modifiers::MouseEvent))
        {
        case Key::Modifiers::MousePress:
            m_dragging = true;
            m_anchor = context.window().buffer_coord(key.coord());
            if (not (key.modifiers & Key::Modifiers::Control))
                context.selections_write_only() = { buffer, m_anchor};
            else
            {
                size_t main = selections.size();
                selections.push_back({m_anchor});
                selections.set_main_index(main);
                selections.sort_and_merge_overlapping();
            }
            return true;

        case Key::Modifiers::MouseRelease:
            if (not m_dragging)
                return true;
            m_dragging = false;
            cursor = context.window().buffer_coord(key.coord());
            selections.main() = {buffer.clamp(m_anchor), cursor};
            selections.sort_and_merge_overlapping();
            return true;

        case Key::Modifiers::MousePos:
            if (not m_dragging)
                return true;
            cursor = context.window().buffer_coord(key.coord());
            selections.main() = {buffer.clamp(m_anchor), cursor};
            selections.sort_and_merge_overlapping();
            return true;

        case Key::Modifiers::MouseWheelDown:
            m_dragging = false;
            scroll_window(context, 3);
            return true;

        case Key::Modifiers::MouseWheelUp:
            m_dragging = false;
            scroll_window(context, -3);
            return true;

        default: return false;
        }
    }

private:
    bool m_dragging = false;
    BufferCoord m_anchor;
};

constexpr StringView register_doc =
    "Special registers:\n"
    "[0-9]: selections capture group\n"
    "%:     buffer name\n"
    ".:     selection contents\n"
    "#:     selection index\n"
    "_:     null register\n"
    "\":     default yank/paste register\n"
    "@:     default macro register\n"
    "/:     default search register\n"
    "^:     default mark register\n"
    "|:     default shell command register\n"
    "::     last entered command\n";

class Normal : public InputMode
{
public:
    Normal(InputHandler& input_handler, bool single_command = false)
        : InputMode(input_handler),
          m_idle_timer{TimePoint::max(),
                       context().flags() & Context::Flags::Transient ?
                           Timer::Callback{} : [this](Timer&) {
              context().hooks().run_hook("NormalIdle", "", context());
          }},
          m_fs_check_timer{TimePoint::max(),
                           context().flags() & Context::Flags::Transient ?
                            Timer::Callback{} : Timer::Callback{[this](Timer& timer) {
              if (context().has_client())
                  context().client().check_if_buffer_needs_reloading();
              timer.set_next_date(Clock::now() + get_fs_check_timeout(context()));
          }}},
          m_single_command(single_command)
    {}

    void on_enabled() override
    {
        if (not (context().flags() & Context::Flags::Transient))
        {
            if (context().has_client())
                context().client().check_if_buffer_needs_reloading();

            m_fs_check_timer.set_next_date(Clock::now() + get_fs_check_timeout(context()));
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
        }

        if (m_hooks_disabled and not m_in_on_key)
        {
            context().hooks_disabled().unset();
            m_hooks_disabled = false;
        }

        context().hooks().run_hook("NormalBegin", "", context());
    }

    void on_disabled(bool temporary) override
    {
        m_idle_timer.set_next_date(TimePoint::max());
        m_fs_check_timer.set_next_date(TimePoint::max());

        if (not temporary and m_hooks_disabled)
        {
            context().hooks_disabled().unset();
            m_hooks_disabled = false;
        }

        context().hooks().run_hook("NormalEnd", "", context());
    }

    void on_key(Key key) override
    {
        ScopedSetBool set_in_on_key{m_in_on_key};

        // Hack to parse keys sent by terminals using the 8th bit to mark the
        // meta key. In normal mode, give priority to a potential alt-key than
        // the accentuated character.
        if (not (key.modifiers & Key::Modifiers::MouseEvent) and
            key.key >= 127 and key.key < 256)
        {
            key.modifiers |= Key::Modifiers::Alt;
            key.key &= 0x7f;
        }

        bool do_restore_hooks = false;
        auto restore_hooks = on_scope_end([&, this]{
            if (m_hooks_disabled and enabled() and do_restore_hooks)
            {
                context().hooks_disabled().unset();
                m_hooks_disabled = false;
            }
        });

        const bool transient = context().flags() & Context::Flags::Transient;

        auto cp = key.codepoint();

        if (m_mouse_handler.handle_key(key, context()))
        {
            context().print_status({});
            if (context().has_client())
                context().client().info_hide();

            if (not transient)
                m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
        }
        else if (cp and isdigit(*cp))
        {
            long long new_val = (long long)m_params.count * 10 + *cp - '0';
            if (new_val > std::numeric_limits<int>::max())
                context().print_status({ "parameter overflowed", get_face("Error") });
            else
                m_params.count = new_val;
        }
        else if (key == Key::Backspace)
            m_params.count /= 10;
        else if (key == '\\')
        {
            if (not m_hooks_disabled)
            {
                m_hooks_disabled = true;
                context().hooks_disabled().set();
            }
        }
        else if (key == '"')
        {
            on_next_key_with_autoinfo(context(), KeymapMode::None,
                [this](Key key, Context& context) {
                    auto cp = key.codepoint();
                    if (not cp or key == Key::Escape)
                        return;
                    if (*cp <= 127)
                        m_params.reg = *cp;
                    else
                        context.print_status(
                            { format("invalid register '{}'", *cp),
                              get_face("Error") });
                }, "enter target register", register_doc);
        }
        else
        {
            // Preserve hooks disabled for the whole execution prior to pop_mode
            ScopedSetBool disable_hooks{context().hooks_disabled(),
                                        m_single_command and m_hooks_disabled};
            if (m_single_command)
                pop_mode();

            context().print_status({});
            if (context().has_client())
                context().client().info_hide();

            do_restore_hooks = true;
            if (auto command = get_normal_command(key))
            {
                auto autoinfo = context().options()["autoinfo"].get<AutoInfo>();
                if (autoinfo & AutoInfo::Normal and context().has_client())
                    context().client().info_show(key_to_str(key), command->docstring.str(),
                                                 {}, InfoStyle::Prompt);

                // reset m_params now to be reentrant
                NormalParams params = m_params;
                m_params = { 0, 0 };

                command->func(context(), params);
            }
        }

        context().hooks().run_hook("NormalKey", key_to_str(key), context());
        if (enabled() and not transient) // The hook might have changed mode
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    DisplayLine mode_line() const override
    {
        AtomList atoms;
        auto num_sel = context().selections().size();
        auto main_index = context().selections().main_index();
        if (num_sel == 1)
            atoms.emplace_back(format("{} sel", num_sel), get_face("StatusLineInfo"));
        else
            atoms.emplace_back(format("{} sels ({})", num_sel, main_index + 1), get_face("StatusLineInfo"));

        if (m_params.count != 0)
        {
            atoms.emplace_back(" param=", get_face("StatusLineInfo"));
            atoms.emplace_back(to_string(m_params.count), get_face("StatusLineValue"));
        }
        if (m_params.reg)
        {
            atoms.emplace_back(" reg=", get_face("StatusLineInfo"));
            atoms.emplace_back(StringView(m_params.reg).str(), get_face("StatusLineValue"));
        }
        return atoms;
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Normal; }

    StringView name() const override { return "normal"; }

private:
    friend struct InputHandler::ScopedForceNormal;

    NormalParams m_params = { 0, 0 };
    bool m_hooks_disabled = false;
    NestedBool m_in_on_key;
    Timer m_idle_timer;
    Timer m_fs_check_timer;
    MouseHandler m_mouse_handler;
    const bool m_single_command;
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
                m_line = m_line.substr(0_char, m_cursor_pos - 1)
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
        else if (key == ctrl('k'))
            m_line = m_line.substr(0_char, m_cursor_pos).str();
        else if (key == ctrl('u'))
        {
            m_line = m_line.substr(m_cursor_pos).str();
            m_cursor_pos = 0;
        }
        else if (auto cp = key.codepoint())
            insert(*cp);
    }

    void insert(Codepoint cp)
    {
        m_line = m_line.substr(0, m_cursor_pos) + String{cp}
               + m_line.substr(m_cursor_pos);
        ++m_cursor_pos;
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

    void reset(String line, StringView empty_text)
    {
        m_line = std::move(line);
        m_empty_text = empty_text;
        m_cursor_pos = m_line.char_length();
        m_display_pos = 0;
    }

    const String& line() const { return m_line; }
    CharCount cursor_pos() const { return m_cursor_pos; }

    ColumnCount cursor_display_column() const
    {
        return m_line.substr(m_display_pos, m_cursor_pos).column_length();
    }

    DisplayLine build_display_line(ColumnCount in_width)
    {
        CharCount width = (int)in_width; // Todo: proper handling of char/column
        kak_assert(m_cursor_pos <= m_line.char_length());
        if (m_cursor_pos < m_display_pos)
            m_display_pos = m_cursor_pos;
        if (m_cursor_pos >= m_display_pos + width)
            m_display_pos = m_cursor_pos + 1 - width;

        const bool empty = m_line.empty();
        StringView str = empty ? m_empty_text : m_line;

        const Face line_face = get_face(empty ? "StatusLineInfo" : "StatusLine");
        const Face cursor_face = get_face("StatusCursor");

        if (m_cursor_pos == str.char_length())
            return DisplayLine{{ { fix_atom_text(str.substr(m_display_pos, width-1)), line_face },
                                 { " "_str, cursor_face} } };
        else
            return DisplayLine({ { fix_atom_text(str.substr(m_display_pos, m_cursor_pos - m_display_pos)), line_face },
                                 { fix_atom_text(str.substr(m_cursor_pos,1)), cursor_face },
                                 { fix_atom_text(str.substr(m_cursor_pos+1, width - m_cursor_pos + m_display_pos - 1)), line_face } });
    }
private:
    CharCount  m_cursor_pos = 0;
    CharCount  m_display_pos = 0;

    String     m_line;
    StringView m_empty_text;
};

class Menu : public InputMode
{
public:
    Menu(InputHandler& input_handler, Vector<DisplayLine> choices,
         MenuCallback callback)
        : InputMode(input_handler),
          m_callback(std::move(callback)), m_choices(choices.begin(), choices.end()),
          m_selected(m_choices.begin())
    {
        if (not context().has_client())
            return;
        context().client().menu_show(std::move(choices), {}, MenuStyle::Prompt);
        context().client().menu_select(0);
    }

    void on_key(Key key) override
    {
        auto match_filter = [this](const DisplayLine& choice) {
            for (auto& atom : choice)
            {
                const auto& contents = atom.content();
                if (regex_match(contents.begin(), contents.end(), m_filter))
                    return true;
            }
            return false;
        };

        if (key == Key::Return)
        {
            if (context().has_client())
                context().client().menu_hide();
            context().print_status(DisplayLine{});

            // Maintain hooks disabled in callback if they were before pop_mode
            ScopedSetBool disable_hooks(context().hooks_disabled(),
                                        context().hooks_disabled());
            pop_mode();
            int selected = m_selected - m_choices.begin();
            m_callback(selected, MenuEvent::Validate, context());
            return;
        }
        else if (key == Key::Escape or key == ctrl('c'))
        {
            if (m_edit_filter)
            {
                m_edit_filter = false;
                m_filter = Regex{".*"};
                m_filter_editor.reset("", "");
                context().print_status(DisplayLine{});
            }
            else
            {
                if (context().has_client())
                    context().client().menu_hide();

                // Maintain hooks disabled in callback if they were before pop_mode
                ScopedSetBool disable_hooks(context().hooks_disabled(),
                                            context().hooks_disabled());
                pop_mode();
                int selected = m_selected - m_choices.begin();
                m_callback(selected, MenuEvent::Abort, context());
            }
        }
        else if (key == Key::Down or key == Key::Tab or
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
            m_filter = Regex{search};
            auto it = std::find_if(m_selected, m_choices.end(), match_filter);
            if (it == m_choices.end())
                it = std::find_if(m_choices.begin(), m_selected, match_filter);
            select(it);
        }

        if (m_edit_filter and context().has_client())
        {
            auto prompt = "filter:"_str;
            auto width = context().client().dimensions().column - prompt.column_length();
            auto display_line = m_filter_editor.build_display_line(width);
            display_line.insert(display_line.begin(), { prompt, get_face("Prompt") });
            context().print_status(display_line);
        }
    }

    DisplayLine mode_line() const override
    {
        return { "menu", get_face("StatusLineMode") };
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Menu; }

    StringView name() const override { return "menu"; }

private:
    MenuCallback m_callback;

    using ChoiceList = Vector<DisplayLine>;
    const ChoiceList m_choices;
    ChoiceList::const_iterator m_selected;

    void select(ChoiceList::const_iterator it)
    {
        m_selected = it;
        int selected = m_selected - m_choices.begin();
        if (context().has_client())
            context().client().menu_select(selected);
        m_callback(selected, MenuEvent::Select, context());
    }

    Regex      m_filter = Regex{".*"};
    bool       m_edit_filter = false;
    LineEditor m_filter_editor;
};

static Optional<Codepoint> get_raw_codepoint(Key key)
{
    if (auto cp = key.codepoint())
        return cp;
    else if (key.modifiers == Key::Modifiers::Control and
             ((key.key >= '@' and key.key <= '_') or
              (key.key >= 'a' and key.key <= 'z')))
        return {(Codepoint)(to_upper((char)key.key) - '@')};
    return {};
}

class Prompt : public InputMode
{
public:
    Prompt(InputHandler& input_handler, StringView prompt,
           String initstr, String emptystr, Face face, PromptFlags flags,
           Completer completer, PromptCallback callback)
        : InputMode(input_handler), m_prompt(prompt.str()), m_prompt_face(face),
          m_empty_text{std::move(emptystr)},
          m_flags(flags), m_completer(std::move(completer)), m_callback(std::move(callback)),
          m_autoshowcompl{context().options()["autoshowcompl"].get<bool>()},
          m_idle_timer{TimePoint::max(), context().flags() & Context::Flags::Transient ?
                           Timer::Callback{} : [this](Timer&) {
                           if (m_autoshowcompl and m_refresh_completion_pending)
                               refresh_completions(CompletionFlags::Fast);
                           if (m_line_changed)
                           {
                               m_callback(m_line_editor.line(), PromptEvent::Change, context());
                               m_line_changed = false;
                           }
                           context().hooks().run_hook("PromptIdle", "", context());
                       }}
    {
        m_history_it = ms_history[m_prompt].end();
        m_line_editor.reset(std::move(initstr), m_empty_text);
    }

    void on_key(Key key) override
    {
        History& history = ms_history[m_prompt];
        const String& line = m_line_editor.line();

        if (key == Key::Return)
        {
            if (not context().history_disabled())
                history_push(history, line);
            context().print_status(DisplayLine{});
            if (context().has_client())
                context().client().menu_hide();

            // Maintain hooks disabled in callback if they were before pop_mode
            ScopedSetBool disable_hooks(context().hooks_disabled(),
                                        context().hooks_disabled());
            pop_mode();
            // call callback after pop_mode so that callback
            // may change the mode
            m_callback(line, PromptEvent::Validate, context());
            return;
        }
        else if (key == Key::Escape or key == ctrl('c'))
        {
            if (not context().history_disabled())
                history_push(history, line);
            context().print_status(DisplayLine{});
            if (context().has_client())
                context().client().menu_hide();

            // Maintain hooks disabled in callback if they were before pop_mode
            ScopedSetBool disable_hooks(context().hooks_disabled(),
                                        context().hooks_disabled());
            pop_mode();
            m_callback(line, PromptEvent::Abort, context());
            return;
        }
        else if (key == ctrl('r'))
        {
            on_next_key_with_autoinfo(context(), KeymapMode::None,
                [this](Key key, Context&) {
                    auto cp = key.codepoint();
                    if (not cp or key == Key::Escape)
                        return;
                    StringView reg = context().main_sel_register_value(String{*cp});
                    m_line_editor.insert(reg);

                    display();
                    m_line_changed = true;
                }, "enter register name", register_doc);
            display();
            return;
        }
        else if (key == ctrl('v'))
        {
            on_next_key_with_autoinfo(context(), KeymapMode::None,
                [this](Key key, Context&) {
                    if (auto cp = get_raw_codepoint(key))
                    {
                        m_line_editor.insert(*cp);
                        display();
                        m_line_changed = true;
                    }
                }, "raw insert", "enter key to insert");
            display();
            return;
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
                        m_line_editor.reset(*it, m_empty_text);
                        break;
                    }
                } while (it != history.begin());

                clear_completions();
                m_refresh_completion_pending = true;
            }
        }
        else if (key == Key::Down or key == ctrl('n')) // next
        {
            if (m_history_it != history.end())
            {
                // search for the next history entry matching typed prefix
                ++m_history_it;
                while (m_history_it != history.end() and
                       not prefix_match(*m_history_it, m_prefix))
                    ++m_history_it;

                if (m_history_it != history.end())
                    m_line_editor.reset(*m_history_it, m_empty_text);
                else
                    m_line_editor.reset(m_prefix, m_empty_text);

                clear_completions();
                m_refresh_completion_pending = true;
            }
        }
        else if (key == Key::Tab or key == Key::BackTab) // tab completion
        {
            const bool reverse = (key == Key::BackTab);
            CandidateList& candidates = m_completions.candidates;
            // first try, we need to ask our completer for completions
            if (candidates.empty())
            {
                refresh_completions(CompletionFlags::None);
                if ((not m_prefix_in_completions and candidates.size() > 1) or
                    candidates.size() > 2)
                    return;
            }

            if (candidates.empty())
                return;

            if (not reverse and ++m_current_completion >= candidates.size())
                m_current_completion = 0;
            else if (reverse and --m_current_completion < 0)
                m_current_completion = candidates.size()-1;

            const String& completion = candidates[m_current_completion];
            if (context().has_client())
                context().client().menu_select(m_current_completion);

            m_line_editor.insert_from(line.char_count_to(m_completions.start),
                                      completion);

            // when we have only one completion candidate, make next tab complete
            // from the new content.
            if (candidates.size() == 1 or
                (m_prefix_in_completions and candidates.size() == 2))
            {
                m_current_completion = -1;
                candidates.clear();
                m_refresh_completion_pending = true;
            }
        }
        else if (key == ctrl('o'))
        {
            m_autoshowcompl = false;
            clear_completions();
            if (context().has_client())
                context().client().menu_hide();
        }
        else
        {
            m_line_editor.handle_key(key);
            clear_completions();
            m_refresh_completion_pending = true;
        }

        display();
        m_line_changed = true;
        if (enabled() and not (context().flags() & Context::Flags::Transient)) // The callback might have disabled us
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
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
        return { "prompt", get_face("StatusLineMode") };
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Prompt; }

    StringView name() const override { return "prompt"; }

    std::pair<CursorMode, DisplayCoord> get_cursor_info() const override
    {
        DisplayCoord coord{0_line, m_prompt.column_length() + m_line_editor.cursor_display_column()};
        return { CursorMode::Prompt, coord };
    }

private:
    void refresh_completions(CompletionFlags flags)
    {
        try
        {
            m_refresh_completion_pending = false;
            if (not m_completer)
                return;
            m_current_completion = -1;
            const String& line = m_line_editor.line();
            m_completions = m_completer(context(), flags, line,
                                        line.byte_count_to(m_line_editor.cursor_pos()));
            if (context().has_client())
            {
                if (m_completions.candidates.empty())
                    return context().client().menu_hide();

                Vector<DisplayLine> items;
                for (auto& candidate : m_completions.candidates)
                    items.push_back({ candidate, {} });
                context().client().menu_show(items, {}, MenuStyle::Prompt);

                auto prefix = line.substr(m_completions.start, m_completions.end - m_completions.start);
                if (not contains(m_completions.candidates, prefix))
                {
                    m_completions.candidates.push_back(prefix.str());
                    m_prefix_in_completions = true;
                }
                else
                    m_prefix_in_completions = false;
            }
        } catch (runtime_error&) {}
    }

    void clear_completions()
    {
        m_current_completion = -1;
        m_completions.candidates.clear();
    }

    void display()
    {
        if (not context().has_client())
            return;

        auto width = context().client().dimensions().column - m_prompt.column_length();
        DisplayLine display_line;
        if (not (m_flags & PromptFlags::Password))
            display_line = m_line_editor.build_display_line(width);
        display_line.insert(display_line.begin(), { m_prompt, m_prompt_face });
        context().print_status(display_line);
    }

    void on_enabled() override
    {
        display();
        m_line_changed = false;
        m_callback(m_line_editor.line(), PromptEvent::Change, context());

        if (not (context().flags() & Context::Flags::Transient))
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    void on_disabled(bool) override
    {
        context().print_status({});
        m_idle_timer.set_next_date(TimePoint::max());
        if (context().has_client())
            context().client().menu_hide();
    }

    PromptCallback m_callback;
    Completer      m_completer;
    const String   m_prompt;
    Face           m_prompt_face;
    Completions    m_completions;
    int            m_current_completion = -1;
    bool           m_prefix_in_completions = false;
    String         m_prefix;
    String         m_empty_text;
    LineEditor     m_line_editor;
    bool           m_line_changed = false;
    PromptFlags    m_flags;
    bool           m_autoshowcompl;
    bool           m_refresh_completion_pending = true;
    Timer          m_idle_timer;

    using History = Vector<String, MemoryDomain::History>;
    static HashMap<String, History, MemoryDomain::History> ms_history;
    History::iterator m_history_it;

    void history_push(History& history, StringView entry)
    {
        if(entry.empty() or
           (m_flags & PromptFlags::DropHistoryEntriesWithBlankPrefix and
            is_horizontal_blank(entry[0_byte])))
            return;

        history.erase(std::remove(history.begin(), history.end(), entry),
                      history.end());
        history.push_back(entry.str());
    }
};
HashMap<String, Prompt::History, MemoryDomain::History> Prompt::ms_history;

class NextKey : public InputMode
{
public:
    NextKey(InputHandler& input_handler, KeymapMode keymap_mode, KeyCallback callback)
        : InputMode(input_handler), m_keymap_mode(keymap_mode), m_callback(std::move(callback)) {}

    void on_key(Key key) override
    {
        // maintain hooks disabled in the callback if they were before pop_mode
        ScopedSetBool disable_hooks(context().hooks_disabled(),
                                    context().hooks_disabled());
        pop_mode();
        m_callback(key, context());
    }

    DisplayLine mode_line() const override
    {
        return { "enter key", get_face("StatusLineMode") };
    }

    KeymapMode keymap_mode() const override { return m_keymap_mode; }

    StringView name() const override { return "next-key"; }

private:
    KeyCallback m_callback;
    KeymapMode m_keymap_mode;
};

class Insert : public InputMode
{
public:
    Insert(InputHandler& input_handler, InsertMode mode, int count)
        : InputMode(input_handler),
          m_restore_cursor(mode == InsertMode::Append),
          m_edition(context()),
          m_completer(context()),
          m_autoshowcompl{context().options()["autoshowcompl"].get<bool>()},
          m_disable_hooks{context().hooks_disabled(), context().hooks_disabled()},
          m_idle_timer{TimePoint::max(), context().flags() & Context::Flags::Transient ?
                       Timer::Callback{} : [this](Timer&) {
                           if (m_autoshowcompl)
                               m_completer.update();
                           context().hooks().run_hook("InsertIdle", "", context());
                       }}
    {
        last_insert().recording.set();
        last_insert().mode = mode;
        last_insert().keys.clear();
        last_insert().disable_hooks = context().hooks_disabled();
        last_insert().count = count;
        context().hooks().run_hook("InsertBegin", "", context());
        prepare(mode, count);

        if (context().has_client() and
            context().options()["readonly"].get<bool>())
            context().print_status({ "Warning: This buffer is readonly",
                                     get_face("Error") });
    }

    void on_enabled() override
    {
        if (not (context().flags() & Context::Flags::Transient))
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    void on_disabled(bool temporary) override
    {
        m_idle_timer.set_next_date(TimePoint::max());

        if (not temporary)
        {
            last_insert().recording.unset();

            auto& selections = context().selections();
            if (m_restore_cursor)
            {
                for (auto& sel : selections)
                {
                    if (sel.cursor() > sel.anchor() and sel.cursor().column > 0)
                        sel.cursor() = context().buffer().char_prev(sel.cursor());
                }
            }
            selections.avoid_eol();
        }
    }

    void on_key(Key key) override
    {
        auto& buffer = context().buffer();

        const bool transient = context().flags() & Context::Flags::Transient;
        bool update_completions = true;
        bool moved = false;
        if (m_mouse_handler.handle_key(key, context()))
        {
            if (not transient)
                m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
        }
        else if (key == Key::Escape or key == ctrl('c'))
        {
            if (m_in_end)
                throw runtime_error("Asked to exit insert mode while running InsertEnd hook");
            m_in_end = true;
            context().hooks().run_hook("InsertEnd", "", context());

            m_completer.reset();
            pop_mode();
        }
        else if (key == Key::Backspace)
        {
            Vector<Selection> sels;
            for (auto& sel : context().selections())
            {
                if (sel.cursor() == BufferCoord{0,0})
                    continue;
                auto pos = sel.cursor();
                sels.emplace_back(buffer.char_prev(pos));
            }
            auto& main = context().selections().main();
            String main_char;
            if (main.cursor() != BufferCoord{0, 0})
                main_char = buffer.string(buffer.char_prev(main.cursor()),
                                          main.cursor());
            if (not sels.empty())
                SelectionList{buffer, std::move(sels)}.erase();

            if (not main_char.empty())
                context().hooks().run_hook("InsertDelete", main_char, context());
        }
        else if (key == Key::Delete)
        {
            Vector<Selection> sels;
            for (auto& sel : context().selections())
                sels.emplace_back(sel.cursor());
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
                sel.anchor() = sel.cursor() = BufferCoord{sel.cursor().line, 0};
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
        else if (auto cp = key.codepoint())
            insert(*cp);
        else if (key == ctrl('r'))
        {
            on_next_key_with_autoinfo(context(), KeymapMode::None,
                [this](Key key, Context&) {
                    auto cp = key.codepoint();
                    if (not cp or key == Key::Escape)
                        return;
                    insert(RegisterManager::instance()[*cp].get(context()));
                }, "enter register name", register_doc);
            update_completions = false;
        }
        else if (key == ctrl('n'))
        {
            last_insert().keys.pop_back();
            m_completer.select(1, last_insert().keys);
            update_completions = false;
        }
        else if (key == ctrl('p'))
        {
            last_insert().keys.pop_back();
            m_completer.select(-1, last_insert().keys);
            update_completions = false;
        }
        else if (key == ctrl('x'))
        {
            on_next_key_with_autoinfo(context(), KeymapMode::None,
                [this](Key key, Context&) {
                    if (key.key == 'f')
                        m_completer.explicit_file_complete();
                    if (key.key == 'w')
                        m_completer.explicit_word_buffer_complete();
                    if (key.key == 'W')
                        m_completer.explicit_word_all_complete();
                    if (key.key == 'l')
                        m_completer.explicit_line_complete();
            }, "enter completion type",
            "f: filename\n"
            "w: word (current buffer)\n"
            "W: word (all buffers)\n"
            "l: line\n");
            update_completions = false;
        }
        else if (key == ctrl('o'))
        {
            m_autoshowcompl = false;
            m_completer.reset();
        }
        else if (key == ctrl('u'))
            context().buffer().commit_undo_group();
        else if (key == ctrl('v'))
        {
            on_next_key_with_autoinfo(context(), KeymapMode::None,
                [this, transient](Key key, Context&) {
                    if (auto cp = get_raw_codepoint(key))
                    {
                        insert(*cp);
                        context().hooks().run_hook("InsertKey", key_to_str(key), context());
                        if (enabled() and not transient)
                            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
                    }
                }, "raw insert", "enter key to insert");
            update_completions = false;
        }
        else if (key == alt(';'))
        {
            push_mode(new Normal(context().input_handler(), true));
            return;
        }

        context().hooks().run_hook("InsertKey", key_to_str(key), context());
        if (moved)
            context().hooks().run_hook("InsertMove", key_to_str(key), context());

        if (update_completions and enabled() and not transient) // Hooks might have disabled us
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    DisplayLine mode_line() const override
    {
        auto num_sel = context().selections().size();
        auto main_index = context().selections().main_index();
        return {AtomList{ { "insert", get_face("StatusLineMode") },
                          { " ", get_face("StatusLine") },
                          { format( "{} sels ({})", num_sel, main_index + 1), get_face("StatusLineInfo") } }};
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Insert; }

    StringView name() const override { return "insert"; }

private:
    template<typename Type>
    void move(Type offset)
    {
        auto& selections = context().selections();
        const ColumnCount tabstop = context().options()["tabstop"].get<int>();
        for (auto& sel : selections)
        {
            auto cursor = context().buffer().offset_coord(sel.cursor(), offset, tabstop, false);
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
        String str{key};
        context().selections().insert(str, InsertMode::InsertCursor);
        context().hooks().run_hook("InsertChar", str, context());
    }

    void prepare(InsertMode mode, int count)
    {
        SelectionList& selections = context().selections();
        Buffer& buffer = context().buffer();

        auto duplicate_selections = [](SelectionList& sels, int count) {
            count = count > 0 ? count : 1;
            Vector<Selection> new_sels;
            new_sels.reserve(count * sels.size());
            for (auto& sel : sels)
                for (int i = 0; i < count; ++i)
                    new_sels.push_back(sel);

            size_t new_main = sels.main_index() * count + count - 1;
            sels = SelectionList{sels.buffer(), std::move(new_sels)};
            sels.set_main_index(new_main);
        };

        switch (mode)
        {
        case InsertMode::Insert:
            for (auto& sel : selections)
                sel.set(sel.max(), sel.min());
            break;
        case InsertMode::Replace:
            selections.erase();
            break;
        case InsertMode::Append:
            for (auto& sel : selections)
            {
                sel.set(sel.min(), sel.max());
                auto& cursor = sel.cursor();
                // special case for end of lines, append to current line instead
                if (cursor.column != buffer[cursor.line].length() - 1)
                    cursor = buffer.char_next(cursor);
            }
            break;
        case InsertMode::AppendAtLineEnd:
            for (auto& sel : selections)
                sel.set({sel.max().line, buffer[sel.max().line].length() - 1});
            break;
        case InsertMode::OpenLineBelow:
            for (auto& sel : selections)
                sel.set({sel.max().line, buffer[sel.max().line].length() - 1});
            duplicate_selections(selections, count);
            insert('\n');
            break;
        case InsertMode::OpenLineAbove:
            for (auto& sel : selections)
                sel.set({sel.min().line});
            duplicate_selections(selections, count);
            // Do not use insert method here as we need to fixup selection
            // before running the InsertChar hook.
            selections.insert("\n"_str, InsertMode::InsertCursor);
            for (auto& sel : selections) // fixup selection positions
                sel.set({sel.cursor().line - 1});
            context().hooks().run_hook("InsertChar", "\n", context());
            break;
        case InsertMode::InsertAtLineBegin:
            for (auto& sel : selections)
            {
                BufferCoord pos = sel.min().line;
                auto pos_non_blank = buffer.iterator_at(pos);
                while (*pos_non_blank == ' ' or *pos_non_blank == '\t')
                    ++pos_non_blank;
                if (*pos_non_blank != '\n')
                    pos = pos_non_blank.coord();
                sel.set(pos);
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

    ScopedEdition   m_edition;
    InsertCompleter m_completer;
    bool            m_restore_cursor;
    bool            m_autoshowcompl;
    Timer           m_idle_timer;
    bool            m_in_end = false;
    MouseHandler    m_mouse_handler;
    ScopedSetBool   m_disable_hooks;
};

}

InputHandler::InputHandler(SelectionList selections, Context::Flags flags, String name)
    : m_context(*this, std::move(selections), flags, std::move(name))
{
    m_mode_stack.emplace_back(new InputModes::Normal(*this));
    current_mode().on_enabled();
}

InputHandler::~InputHandler() = default;

void InputHandler::push_mode(InputMode* new_mode)
{
    StringView prev_name = current_mode().name();

    current_mode().on_disabled(true);
    m_mode_stack.emplace_back(new_mode);
    new_mode->on_enabled();

    context().hooks().run_hook("InputModeChange", format("{}:{}", prev_name, new_mode->name()), context());
}

void InputHandler::pop_mode(InputMode* mode)
{
    kak_assert(m_mode_stack.back().get() == mode);
    kak_assert(m_mode_stack.size() > 1);

    StringView prev_name = mode->name();

    current_mode().on_disabled(false);
    m_mode_stack.pop_back();
    current_mode().on_enabled();

    context().hooks().run_hook("InputModeChange", format("{}:{}", prev_name, current_mode().name()), context());
}

void InputHandler::reset_normal_mode()
{
    kak_assert(dynamic_cast<InputModes::Normal*>(m_mode_stack[0].get()) != nullptr);
    if (m_mode_stack.size() == 1)
        return;

    StringView prev_name = current_mode().name();
    current_mode().on_disabled(false);
    m_mode_stack.resize(1);
    current_mode().on_enabled();

    context().hooks().run_hook("InputModeChange", format("{}:{}", prev_name, current_mode().name()), context());
}

void InputHandler::insert(InsertMode mode, int count)
{
    push_mode(new InputModes::Insert(*this, mode, count));
}

void InputHandler::repeat_last_insert()
{
    if (m_last_insert.keys.empty())
        return;

    if (dynamic_cast<InputModes::Normal*>(&current_mode()) == nullptr or
        m_last_insert.recording)
        throw runtime_error{"repeating last insert not available in this context"};

    Vector<Key> keys;
    swap(keys, m_last_insert.keys);
    ScopedSetBool disable_hooks(context().hooks_disabled(),
                                m_last_insert.disable_hooks);

    push_mode(new InputModes::Insert(*this, m_last_insert.mode, m_last_insert.count));
    for (auto& key : keys)
    {
        // refill last_insert,  this is very inefficient, but necesary at the moment
        // to properly handle insert completion
        m_last_insert.keys.push_back(key);
        current_mode().handle_key(key);
    }
    kak_assert(dynamic_cast<InputModes::Normal*>(&current_mode()) != nullptr);
}

void InputHandler::prompt(StringView prompt, String initstr, String emptystr,
                          Face prompt_face, PromptFlags flags,
                          Completer completer, PromptCallback callback)
{
    push_mode(new InputModes::Prompt(*this, prompt, std::move(initstr), std::move(emptystr),
                                     prompt_face, flags, std::move(completer), std::move(callback)));
}

void InputHandler::set_prompt_face(Face prompt_face)
{
    InputModes::Prompt* prompt = dynamic_cast<InputModes::Prompt*>(&current_mode());
    if (prompt)
        prompt->set_prompt_face(prompt_face);
}

void InputHandler::menu(Vector<DisplayLine> choices, MenuCallback callback)
{
    push_mode(new InputModes::Menu(*this, std::move(choices), std::move(callback)));
}

void InputHandler::on_next_key(KeymapMode keymap_mode, KeyCallback callback)
{
    push_mode(new InputModes::NextKey(*this, keymap_mode, std::move(callback)));
}

InputHandler::ScopedForceNormal::ScopedForceNormal(InputHandler& handler, NormalParams params)
    : m_handler(handler), m_mode(nullptr)
{
    if (handler.m_mode_stack.size() != 1)
    {
        handler.push_mode(new InputModes::Normal(handler));
        m_mode = handler.m_mode_stack.back().get();
    }

    static_cast<InputModes::Normal*>(handler.m_mode_stack.back().get())->m_params = params;
}

InputHandler::ScopedForceNormal::~ScopedForceNormal()
{
    if (not m_mode)
        return;

    kak_assert(m_handler.m_mode_stack.size() > 1);

    if (m_mode == m_handler.m_mode_stack.back().get())
        m_handler.pop_mode(m_mode);
    else
    {
        auto it = find_if(m_handler.m_mode_stack,
                          [this](const RefPtr<InputMode>& m)
                          { return m.get() == m_mode; });
        kak_assert(it != m_handler.m_mode_stack.end());
        m_handler.m_mode_stack.erase(it);
    }
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
        auto dec = on_scope_end([this]{ --m_handle_key_level; });

        auto process_key = [&](Key key) {
            if (m_last_insert.recording)
                m_last_insert.keys.push_back(key);
            current_mode().handle_key(key);
        };

        auto keymap_mode = current_mode().keymap_mode();
        KeymapManager& keymaps = m_context.keymaps();
        if (keymaps.is_mapped(key, keymap_mode) and
            not m_context.keymaps_disabled())
        {
            ScopedSetBool disable_history{context().history_disabled()};
            for (auto& k : keymaps.get_mapping(key, keymap_mode).keys)
                process_key(k);
        }
        else
            process_key(key);

        // do not record the key that made us enter or leave recording mode,
        // and the ones that are triggered recursively by previous keys.
        if (was_recording and is_recording() and m_handle_key_level == m_recording_level)
            m_recorded_keys += key_to_str(key);

        if (m_handle_key_level < m_recording_level)
        {
            write_to_debug_buffer("Macro recording started but not finished");
            m_recording_reg = 0;
            m_recording_level = -1;
        }
    }
}

void InputHandler::start_recording(char reg)
{
    kak_assert(m_recording_reg == 0);
    m_recording_level = m_handle_key_level;
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

    if (not m_recorded_keys.empty())
        RegisterManager::instance()[m_recording_reg].set(
            context(), {m_recorded_keys});

    m_recording_reg = 0;
    m_recording_level = -1;
}

DisplayLine InputHandler::mode_line() const
{
    return current_mode().mode_line();
}

std::pair<CursorMode, DisplayCoord> InputHandler::get_cursor_info() const
{
    return current_mode().get_cursor_info();
}

bool show_auto_info_ifn(StringView title, StringView info, AutoInfo mask, const Context& context)
{
    if (not (context.options()["autoinfo"].get<AutoInfo>() & mask) or
        not context.has_client())
        return false;

    context.client().info_show(title.str(), info.str(), {}, InfoStyle::Prompt);
    return true;
}

void hide_auto_info_ifn(const Context& context, bool hide)
{
    if (hide)
        context.client().info_hide();
}

void scroll_window(Context& context, LineCount offset)
{
    Window& window = context.window();
    Buffer& buffer = context.buffer();

    DisplayCoord win_pos = window.position();
    DisplayCoord win_dim = window.dimensions();

    const DisplayCoord max_offset{(win_dim.line - 1)/2, (win_dim.column - 1)/2};
    const DisplayCoord scrolloff =
        std::min(context.options()["scrolloff"].get<DisplayCoord>(), max_offset);

    const LineCount line_count = buffer.line_count();
    win_pos.line = clamp(win_pos.line + offset, 0_line, line_count-1);

    SelectionList& selections = context.selections();
    const BufferCoord cursor = selections.main().cursor();

    auto line = clamp(cursor.line, win_pos.line + scrolloff.line,
                      win_pos.line + win_dim.line - 1 - scrolloff.line);
    line = clamp(line, 0_line, line_count-1);

    using std::min; using std::max;
    // This is not exactly a clamp, and must be done in this order as
    // byte_count_to could return line length
    auto col = min(max(cursor.column, buffer[line].byte_count_to(win_pos.column)),
                   buffer[line].length()-1);

    selections = SelectionList{buffer, BufferCoord{line, col}};
    window.set_position(win_pos);
}

}
