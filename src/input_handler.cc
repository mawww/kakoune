#include "input_handler.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "command_manager.hh"
#include "client.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "insert_completer.hh"
#include "normal.hh"
#include "option_types.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "hash_map.hh"
#include "user_interface.hh"
#include "utf8.hh"
#include "window.hh"
#include "word_db.hh"

#include <utility>
#include <limits>

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
        constexpr auto modifiers = Key::Modifiers::Control | Key::Modifiers::Alt | Key::Modifiers::Shift | Key::Modifiers::MouseButtonMask;
        switch ((key.modifiers & ~modifiers).value)
        {
        case Key::Modifiers::MousePress:
            switch (key.mouse_button())
            {
            case Key::MouseButton::Right:
                m_dragging = false;
                cursor = context.window().buffer_coord(key.coord());
                if (key.modifiers & Key::Modifiers::Control)
                    selections = {{selections.begin()->anchor(), cursor}};
                else
                    selections.main() = {selections.main().anchor(), cursor};
                selections.sort_and_merge_overlapping();
                return true;

            case Key::MouseButton::Left:
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

            default: return true;
            }

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

        case Key::Modifiers::Scroll:
            scroll_window(context, static_cast<int32_t>(key.key), m_dragging);
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
                       context().flags() & Context::Flags::Draft ?
                           Timer::Callback{} : [this](Timer&) {
              RefPtr<InputMode> keep_alive{this}; // hook could trigger pop_mode()
              context().hooks().run_hook(Hook::NormalIdle, "", context());
          }},
          m_fs_check_timer{TimePoint::max(),
                           context().flags() & Context::Flags::Draft ?
                            Timer::Callback{} : Timer::Callback{[this](Timer& timer) {
              if (context().has_client())
                  context().client().check_if_buffer_needs_reloading();
              timer.set_next_date(Clock::now() + get_fs_check_timeout(context()));
          }}},
          m_state(single_command ? State::SingleCommand : State::Normal)
    {}

    void on_enabled() override
    {
        if (m_state == State::PopOnEnabled)
            return pop_mode();

        if (not (context().flags() & Context::Flags::Draft))
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
    }

    void on_disabled(bool temporary) override
    {
        m_idle_timer.disable();
        m_fs_check_timer.disable();

        if (not temporary and m_hooks_disabled)
        {
            context().hooks_disabled().unset();
            m_hooks_disabled = false;
        }
    }

    void on_key(Key key) override
    {
        kak_assert(m_state != State::PopOnEnabled);
        ScopedSetBool set_in_on_key{m_in_on_key};

        bool do_restore_hooks = false;
        auto restore_hooks = on_scope_end([&, this]{
            if (m_hooks_disabled and enabled() and do_restore_hooks)
            {
                context().hooks_disabled().unset();
                m_hooks_disabled = false;
            }
        });

        const bool transient = context().flags() & Context::Flags::Draft;

        if (m_mouse_handler.handle_key(key, context()))
        {
            context().print_status({});
            if (context().has_client())
                context().client().info_hide();

            if (not transient)
                m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
        }
        else if (auto cp = key.codepoint(); cp and isdigit(*cp))
        {
            long long new_val = (long long)m_params.count * 10 + *cp - '0';
            if (new_val > std::numeric_limits<int>::max())
                context().print_status({ "parameter overflowed", context().faces()["Error"] });
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
            on_next_key_with_autoinfo(context(), "register", KeymapMode::None,
                [this](Key key, Context& context) {
                    auto cp = key.codepoint();
                    if (not cp or key == Key::Escape)
                        return;
                    if (*cp <= 127)
                        m_params.reg = *cp;
                    else
                        context.print_status(
                            { format("invalid register '{}'", *cp),
                              context.faces()["Error"] });
                }, "enter target register", register_doc.str());
        }
        else
        {
            auto pop_if_single_command = on_scope_end([this] {
                if (m_state == State::SingleCommand and enabled())
                     pop_mode();
                else if (m_state == State::SingleCommand)
                     m_state = State::PopOnEnabled;
            });

            context().print_status({});
            if (context().has_client())
                context().client().info_hide();

            // Hack to parse keys sent by terminals using the 8th bit to mark the
            // meta key. In normal mode, give priority to a potential alt-key than
            // the accentuated character.
            if (key.key >= 127 and key.key < 256)
            {
                key.modifiers |= Key::Modifiers::Alt;
                key.key &= 0x7f;
            }

            do_restore_hooks = true;
            if (auto command = get_normal_command(key))
            {
                auto autoinfo = context().options()["autoinfo"].get<AutoInfo>();
                if (autoinfo & AutoInfo::Normal and context().has_client())
                    context().client().info_show(to_string(key), command->docstring.str(),
                                                 {}, InfoStyle::Prompt);

                // reset m_params now to be reentrant
                NormalParams params = m_params;
                m_params = { 0, 0 };

                command->func(context(), params);
            }
            else
                m_params = { 0, 0 };
        }

        context().hooks().run_hook(Hook::NormalKey, to_string(key), context());
        if (enabled() and not transient) // The hook might have changed mode
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    DisplayLine mode_line() const override
    {
        AtomList atoms;
        auto num_sel = context().selections().size();
        auto main_index = context().selections().main_index();
        if (num_sel == 1)
            atoms.emplace_back(format("{} sel", num_sel), context().faces()["StatusLineInfo"]);
        else
            atoms.emplace_back(format("{} sels ({})", num_sel, main_index + 1), context().faces()["StatusLineInfo"]);

        if (m_params.count != 0)
        {
            atoms.emplace_back(" param=", context().faces()["StatusLineInfo"]);
            atoms.emplace_back(to_string(m_params.count), context().faces()["StatusLineValue"]);
        }
        if (m_params.reg)
        {
            atoms.emplace_back(" reg=", context().faces()["StatusLineInfo"]);
            atoms.emplace_back(StringView(m_params.reg).str(), context().faces()["StatusLineValue"]);
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

    enum class State { Normal, SingleCommand, PopOnEnabled };
    State m_state;
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
    LineEditor(const FaceRegistry& faces) : m_faces{faces} {}

    void handle_key(Key key)
    {
        auto erase_move = [this](auto&& move_func) {
            auto old_pos = m_cursor_pos;
            move_func(m_cursor_pos, m_line);
            if (m_cursor_pos > old_pos)
                std::swap(m_cursor_pos, old_pos);
            m_clipboard = m_line.substr(m_cursor_pos, old_pos - m_cursor_pos).str();
            m_line = m_line.substr(0, m_cursor_pos) + m_line.substr(old_pos);
        };

        if (key == Key::Left or key == ctrl('b'))
        {
            if (m_cursor_pos > 0)
                --m_cursor_pos;
        }
        else if (key == Key::Right or key == ctrl('f'))
        {
            if (m_cursor_pos < m_line.char_length())
                ++m_cursor_pos;
        }
        else if (key == Key::Home or key == ctrl('a'))
            m_cursor_pos = 0;
        else if (key == Key::End or key == ctrl('e'))
            m_cursor_pos = m_line.char_length();
        else if (key == Key::Backspace or key == shift(Key::Backspace) or key == ctrl('h'))
        {
            if (m_cursor_pos != 0)
            {
                m_line = m_line.substr(0_char, m_cursor_pos - 1)
                       + m_line.substr(m_cursor_pos);

                --m_cursor_pos;
            }
        }
        else if (key == Key::Delete or key == ctrl('d'))
        {
            if (m_cursor_pos != m_line.char_length())
                m_line = m_line.substr(0, m_cursor_pos)
                       + m_line.substr(m_cursor_pos+1);
        }
        else if (key == alt('f'))
            to_next_word_begin<Word>(m_cursor_pos, m_line);
        else if (key == alt('F'))
            to_next_word_begin<WORD>(m_cursor_pos, m_line);
        else if (key == alt('b'))
            to_prev_word_begin<Word>(m_cursor_pos, m_line);
        else if (key == alt('B'))
            to_prev_word_begin<WORD>(m_cursor_pos, m_line);
        else if (key == alt('e'))
            to_next_word_end<Word>(m_cursor_pos, m_line);
        else if (key == alt('E'))
            to_next_word_end<WORD>(m_cursor_pos, m_line);
        else if (key == ctrl('k'))
        {
            m_clipboard = m_line.substr(m_cursor_pos).str();
            m_line = m_line.substr(0, m_cursor_pos).str();
        }
        else if (key == ctrl('u'))
        {
            m_clipboard = m_line.substr(0, m_cursor_pos).str();
            m_line = m_line.substr(m_cursor_pos).str();
            m_cursor_pos = 0;
        }
        else if (key == ctrl('w') or key == alt(Key::Backspace))
            erase_move(&to_prev_word_begin<Word>);
        else if (key == ctrl('W'))
            erase_move(&to_prev_word_begin<WORD>);
        else if (key == alt('d'))
            erase_move(&to_next_word_begin<Word>);
        else if (key == alt('D'))
            erase_move(&to_next_word_begin<WORD>);
        else if (key == ctrl('y'))
        {
            m_line = m_line.substr(0, m_cursor_pos) + m_clipboard + m_line.substr(m_cursor_pos);
            m_cursor_pos += m_clipboard.char_length();
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

        const Face line_face = m_faces[empty ? "StatusLineInfo" : "StatusLine"];
        const Face cursor_face = m_faces["StatusCursor"];

        if (m_cursor_pos == str.char_length())
            return DisplayLine{{ { str.substr(m_display_pos, width-1).str(), line_face },
                                 { " "_str, cursor_face} } };
        else
            return DisplayLine({ { str.substr(m_display_pos, m_cursor_pos - m_display_pos).str(), line_face },
                                 { str.substr(m_cursor_pos,1).str(), cursor_face },
                                 { str.substr(m_cursor_pos+1, width - m_cursor_pos + m_display_pos - 1).str(), line_face } });
    }
private:
    CharCount  m_cursor_pos = 0;
    CharCount  m_display_pos = 0;

    String     m_line;
    StringView m_empty_text = {};
    String     m_clipboard;

    const FaceRegistry& m_faces;
};

class Menu : public InputMode
{
public:
    Menu(InputHandler& input_handler, Vector<DisplayLine> choices,
         MenuCallback callback)
        : InputMode(input_handler),
          m_callback(std::move(callback)), m_choices(choices.begin(), choices.end()),
          m_selected(m_choices.begin()),
          m_filter_editor{context().faces()}
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
        else if (key == Key::Up or key == shift(Key::Tab) or
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
            display_line.insert(display_line.begin(), { prompt, context().faces()["Prompt"] });
            context().print_status(display_line);
        }
    }

    DisplayLine mode_line() const override
    {
        return { "menu", context().faces()["StatusLineMode"] };
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
           char history_register, PromptCompleter completer, PromptCallback callback)
        : InputMode(input_handler), m_callback(std::move(callback)), m_completer(std::move(completer)),
          m_prompt(prompt.str()), m_prompt_face(face),
          m_empty_text{std::move(emptystr)},
          m_line_editor{context().faces()}, m_flags(flags),
          m_history{RegisterManager::instance()[history_register]},
          m_current_history{-1},
          m_auto_complete{context().options()["autocomplete"].get<AutoComplete>() & AutoComplete::Prompt},
          m_idle_timer{TimePoint::max(), context().flags() & Context::Flags::Draft ?
                           Timer::Callback{} : [this](Timer&) {
                           RefPtr<InputMode> keep_alive{this}; // hook or m_callback could trigger pop_mode()
                           if (m_auto_complete and m_refresh_completion_pending)
                               refresh_completions(CompletionFlags::Fast);
                           if (m_line_changed)
                           {
                               m_callback(m_line_editor.line(), PromptEvent::Change, context());
                               m_line_changed = false;
                           }
                           context().hooks().run_hook(Hook::PromptIdle, "", context());
                       }}
    {
        m_line_editor.reset(std::move(initstr), m_empty_text);
    }

    void on_key(Key key) override
    {
        const String& line = m_line_editor.line();

        auto can_auto_insert_completion = [&] {
            const bool has_completions = not m_completions.candidates.empty();
            const bool completion_selected = m_current_completion != -1;
            const bool text_entered = m_completions.start != line.byte_count_to(m_line_editor.cursor_pos());
            return (m_completions.flags & Completions::Flags::Menu) and
                has_completions and
                not completion_selected and
                (not (m_completions.flags & Completions::Flags::NoEmpty) or text_entered);
        };

        if (key == Key::Return)
        {
            if (can_auto_insert_completion())
            {
                const String& completion = m_completions.candidates.front();
                m_line_editor.insert_from(line.char_count_to(m_completions.start),
                                          completion);
            }

            history_push(line);
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
        else if (key == Key::Escape or key == ctrl('c') or
                 ((key == Key::Backspace or key == shift(Key::Backspace) or key == ctrl('h')) and line.empty()))
        {
            history_push(line);
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
            on_next_key_with_autoinfo(context(), "register", KeymapMode::None,
                [this](Key key, Context&) {
                    const bool joined = (bool)(key.modifiers & Key::Modifiers::Alt);
                    key.modifiers &= ~Key::Modifiers::Alt;

                    auto cp = key.codepoint();
                    if (not cp or key == Key::Escape)
                        return;

                    m_line_editor.insert(
                        joined ? join(RegisterManager::instance()[*cp].get(context()), ' ', false)
                               : context().main_sel_register_value(String{*cp}));

                    display();
                    m_line_changed = true;
                    m_refresh_completion_pending = true;
                }, "enter register name", register_doc.str());
            display();
            return;
        }
        else if (key == ctrl('v'))
        {
            on_next_key_with_autoinfo(context(), "raw-key", KeymapMode::None,
                [this](Key key, Context&) {
                    if (auto cp = get_raw_codepoint(key))
                    {
                        m_line_editor.insert(*cp);
                        display();
                        m_line_changed = true;
                        m_refresh_completion_pending = true;
                    }
                }, "raw insert", "enter key to insert");
            display();
            return;
        }
        else if (key == Key::Up or key == ctrl('p'))
        {
            auto history = m_history.get(context());
            m_current_history = std::min(static_cast<int>(history.size()) - 1, m_current_history);
            if (m_current_history == -1)
               m_prefix = line;
            auto next = find_if(history.subrange(m_current_history + 1), [this](StringView s) { return prefix_match(s, m_prefix); });
            if (next != history.end())
            {
                m_current_history = next - history.begin();
                m_line_editor.reset(*next, m_empty_text);
            }
            clear_completions();
            m_refresh_completion_pending = true;
        }
        else if (key == Key::Down or key == ctrl('n')) // next
        {
            auto history = m_history.get(context());
            m_current_history = std::min(static_cast<int>(history.size()) - 1, m_current_history);
            if (m_current_history >= 0)
            {
                auto next = find_if(history.subrange(0, m_current_history) | reverse(), [this](StringView s) { return prefix_match(s, m_prefix); });
                m_current_history = history.rend() - next - 1;
                m_line_editor.reset(next != history.rend() ? *next : m_prefix, m_empty_text);
                clear_completions();
                m_refresh_completion_pending = true;
            }
        }
        else if (key == Key::Tab or key == shift(Key::Tab) or key.modifiers == Key::Modifiers::MenuSelect) // completion
        {
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

            const bool reverse = (key == shift(Key::Tab));
            if (key.modifiers == Key::Modifiers::MenuSelect)
                m_current_completion = clamp<int>(key.key, 0, candidates.size() - 1);
            else if (not reverse and ++m_current_completion >= candidates.size())
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
        else if (key == ctrl('x'))
        {
            on_next_key_with_autoinfo(context(), "explicit-completion", KeymapMode::None,
                [this](Key key, Context&) {
                    m_explicit_completer = PromptCompleter{};

                    if (key.key == 'f')
                        use_explicit_completer([](const Context& context, StringView token) {
                            return complete_filename(token, context.options()["ignored_files"].get<Regex>(),
                                                     token.length(), FilenameFlags::Expand);
                        });
                    else if (key.key == 'w')
                        use_explicit_completer([](const Context& context, StringView token) {
                            CandidateList candidates;
                            for_n_best(get_word_db(context.buffer()).find_matching(token),
                                       100, [](auto& lhs, auto& rhs){ return rhs < lhs; },
                                       [&](RankedMatch& m) {
                                candidates.push_back(m.candidate().str());
                                return true;
                            });
                            return candidates;
                        });

                    if (m_explicit_completer)
                        refresh_completions(CompletionFlags::None);
                }, "enter completion type",
                "f: filename\n"
                "w: buffer word\n");
        }
        else if (key == ctrl('o'))
        {
            m_explicit_completer = PromptCompleter{};
            m_auto_complete = not m_auto_complete;

            if (m_auto_complete)
                refresh_completions(CompletionFlags::Fast);
            else if (context().has_client())
            {
                clear_completions();
                context().client().menu_hide();
            }
        }
        else if (key == alt('!'))
        {
            try
            {
                m_line_editor.reset(expand(line, context()), m_empty_text);
            }
            catch (std::runtime_error& error)
            {
                context().print_status({error.what(), context().faces()["Error"]});
                return;
            }
        }
        else if (key == alt(';'))
        {
            push_mode(new Normal(context().input_handler(), true));
            return;
        }
        else
        {
            if ((key == Key::Space or key == shift(Key::Space)) and
                not (m_completions.flags & Completions::Flags::Quoted) and // if token is quoted, this space does not end it
                can_auto_insert_completion())
                m_line_editor.insert_from(line.char_count_to(m_completions.start),
                                          m_completions.candidates.front());

            m_line_editor.handle_key(key);
            clear_completions();
            m_refresh_completion_pending = true;
        }

        display();
        m_line_changed = true;
        if (enabled() and not (context().flags() & Context::Flags::Draft)) // The callback might have disabled us
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
        return { "prompt", context().faces()["StatusLineMode"] };
    }

    KeymapMode keymap_mode() const override { return KeymapMode::Prompt; }

    StringView name() const override { return "prompt"; }

    std::pair<CursorMode, DisplayCoord> get_cursor_info() const override
    {
        DisplayCoord coord{0_line, m_prompt.column_length() + m_line_editor.cursor_display_column()};
        return { CursorMode::Prompt, coord };
    }

private:
    template<typename Completer>
    void use_explicit_completer(Completer&& completer)
    {
        m_explicit_completer = [completer](const Context& context, CompletionFlags flags, StringView content, ByteCount cursor_pos) {
            Optional<Token> last_token;
            CommandParser parser{content.substr(0_byte, cursor_pos)};
            while (auto token = parser.read_token(false))
                last_token = std::move(token);

            if (last_token and (last_token->pos + last_token->content.length() < cursor_pos))
                last_token.reset();

            auto token_start = last_token.map([&](auto&& t) { return t.pos; }).value_or(cursor_pos);
            auto token_content = last_token.map([](auto&& t) -> StringView { return t.content; }).value_or(StringView{});

            return Completions{token_start, cursor_pos, completer(context, token_content)};
        };
    }

    void refresh_completions(CompletionFlags flags)
    {
        try
        {
            m_refresh_completion_pending = false;
            auto& completer = m_explicit_completer ? m_explicit_completer : m_completer;
            if (not completer)
                return;
            m_current_completion = -1;
            const String& line = m_line_editor.line();
            m_completions = completer(context(), flags, line,
                                      line.byte_count_to(m_line_editor.cursor_pos()));
            const bool menu = (bool)(m_completions.flags & Completions::Flags::Menu);
            if (context().has_client())
            {
                if (m_completions.candidates.empty())
                    return context().client().menu_hide();

                Vector<DisplayLine> items;
                for (auto& candidate : m_completions.candidates)
                    items.push_back({ candidate, {} });

                const auto menu_style = (m_flags & PromptFlags::Search) ? MenuStyle::Search : MenuStyle::Prompt;
                context().client().menu_show(items, {}, menu_style);

                if (menu)
                    context().client().menu_select(0);

                auto prefix = line.substr(m_completions.start, m_completions.end - m_completions.start);
                if (not menu and not contains(m_completions.candidates, prefix))
                {
                    m_current_completion = m_completions.candidates.size();
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
        m_line_changed = true;

        if (not (context().flags() & Context::Flags::Draft))
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    void on_disabled(bool temporary) override
    {
        if (not temporary)
            context().print_status({});

        m_idle_timer.disable();
        if (context().has_client())
            context().client().menu_hide();
    }

    PromptCallback  m_callback;
    PromptCompleter m_completer;
    PromptCompleter m_explicit_completer;
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
    Register&      m_history;
    int            m_current_history;
    bool           m_auto_complete;
    bool           m_refresh_completion_pending = true;
    Timer          m_idle_timer;

    void history_push(StringView entry)
    {
        if (entry.empty() or context().history_disabled() or
            (m_flags & PromptFlags::DropHistoryEntriesWithBlankPrefix and
             is_horizontal_blank(entry[0_byte])))
            return;

        m_history.set(context(), {entry.str()});
    }
};

class NextKey : public InputMode
{
public:
    NextKey(InputHandler& input_handler, String name, KeymapMode keymap_mode, KeyCallback callback,
            Timer::Callback idle_callback)
        : InputMode(input_handler), m_name{std::move(name)}, m_callback(std::move(callback)), m_keymap_mode(keymap_mode),
          m_idle_timer{Clock::now() + get_idle_timeout(context()), std::move(idle_callback)} {}

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
        return { "enter key", context().faces()["StatusLineMode"] };
    }

    KeymapMode keymap_mode() const override { return m_keymap_mode; }

    StringView name() const override { return m_name; }

private:
    String         m_name;
    KeyCallback    m_callback;
    KeymapMode     m_keymap_mode;
    Timer          m_idle_timer;
};

class Insert : public InputMode
{
public:
    Insert(InputHandler& input_handler, InsertMode mode, int count)
        : InputMode(input_handler),
          m_edition(context()),
          m_completer(context()),
          m_restore_cursor(mode == InsertMode::Append),
          m_auto_complete{context().options()["autocomplete"].get<AutoComplete>() & AutoComplete::Insert},
          m_idle_timer{TimePoint::max(), context().flags() & Context::Flags::Draft ?
                       Timer::Callback{} : [this](Timer&) {
                           RefPtr<InputMode> keep_alive{this}; // hook could trigger pop_mode()
                           m_completer.update(m_auto_complete);
                           context().hooks().run_hook(Hook::InsertIdle, "", context());
                       }},
          m_disable_hooks{context().hooks_disabled(), context().hooks_disabled()}
    {
        context().buffer().throw_if_read_only();

        last_insert().recording.set();
        last_insert().mode = mode;
        last_insert().keys.clear();
        last_insert().disable_hooks = context().hooks_disabled();
        last_insert().count = count;
        prepare(mode, count);
    }

    void on_enabled() override
    {
        if (not (context().flags() & Context::Flags::Draft))
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    void on_disabled(bool temporary) override
    {
        m_idle_timer.disable();

        if (not temporary)
        {
            last_insert().recording.unset();

            auto& selections = context().selections();
            if (m_restore_cursor)
            {
                for (auto& sel : selections)
                {
                    if (sel.cursor() > sel.anchor() and sel.cursor() > BufferCoord{0, 0})
                        sel.cursor() = context().buffer().char_prev(sel.cursor());
                }
            }
        }
    }

    void on_key(Key key) override
    {
        auto& buffer = context().buffer();

        const bool transient = context().flags() & Context::Flags::Draft;
        bool update_completions = true;
        bool moved = false;
        if (m_mouse_handler.handle_key(key, context()))
        {
            if (not transient)
                m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
        }
        else if (key == Key::Escape or key == ctrl('c'))
        {
            m_completer.reset();
            pop_mode();
        }
        else if (key == Key::Backspace or key == shift(Key::Backspace))
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
                context().hooks().run_hook(Hook::InsertDelete, main_char, context());

            context().selections_write_only().update(false);
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
            on_next_key_with_autoinfo(context(), "register", KeymapMode::None,
                [this](Key key, Context&) {
                    auto cp = key.codepoint();
                    if (not cp or key == Key::Escape)
                        return;
                    insert(RegisterManager::instance()[*cp].get(context()));
                }, "enter register name", register_doc.str());
            update_completions = false;
        }
        else if (key == ctrl('n'))
        {
            last_insert().keys.pop_back();
            m_completer.select(1, true, last_insert().keys);
            update_completions = false;
        }
        else if (key == ctrl('p'))
        {
            last_insert().keys.pop_back();
            m_completer.select(-1, true, last_insert().keys);
            update_completions = false;
        }
        else if (key.modifiers == Key::Modifiers::MenuSelect)
        {
            last_insert().keys.pop_back();
            m_completer.select(key.key, false, last_insert().keys);
            update_completions = false;
        }
        else if (key == ctrl('x'))
        {
            on_next_key_with_autoinfo(context(), "explicit-completion", KeymapMode::None,
                [this](Key key, Context&) {
                    if (key.key == 'f')
                        m_completer.explicit_file_complete();
                    if (key.key == 'w')
                        m_completer.explicit_word_buffer_complete();
                    if (key.key == 'W')
                        m_completer.explicit_word_all_complete();
                    if (key.key == 'l')
                        m_completer.explicit_line_buffer_complete();
                    if (key.key == 'L')
                        m_completer.explicit_line_all_complete();
            }, "enter completion type",
            "f: filename\n"
            "w: word (current buffer)\n"
            "W: word (all buffers)\n"
            "l: line (current buffer)\n"
            "L: line (all buffers)\n");
            update_completions = false;
        }
        else if (key == ctrl('o'))
        {
            m_auto_complete = not m_auto_complete;
            m_completer.reset();
        }
        else if (key == ctrl('u'))
        {
            context().buffer().commit_undo_group();
            context().print_status({ format("committed change #{}",
                                            (size_t)context().buffer().current_history_id()),
                                     context().faces()["Information"] });
        }
        else if (key == ctrl('v'))
        {
            on_next_key_with_autoinfo(context(), "raw-insert", KeymapMode::None,
                [this, transient](Key key, Context&) {
                    if (auto cp = get_raw_codepoint(key))
                    {
                        insert(*cp);
                        context().hooks().run_hook(Hook::InsertKey, to_string(key), context());
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

        context().hooks().run_hook(Hook::InsertKey, to_string(key), context());
        if (moved)
            context().hooks().run_hook(Hook::InsertMove, to_string(key), context());

        if (update_completions and enabled() and not transient) // Hooks might have disabled us
            m_idle_timer.set_next_date(Clock::now() + get_idle_timeout(context()));
    }

    DisplayLine mode_line() const override
    {
        auto num_sel = context().selections().size();
        auto main_index = context().selections().main_index();
        return {AtomList{ { "insert", context().faces()["StatusLineMode"] },
                          { " ", context().faces()["StatusLine"] },
                          { num_sel == 1 ? format("{} sel", num_sel)
                              : format("{} sels ({})", num_sel, main_index + 1),
                            context().faces()["StatusLineInfo"] } }};
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
            auto cursor = context().buffer().offset_coord(sel.cursor(), offset, tabstop);
            sel.anchor() = sel.cursor() = cursor;
        }
        selections.sort_and_merge_overlapping();
    }

    void insert(ConstArrayView<String> strings)
    {
        m_completer.try_accept();
        context().selections().for_each([strings, &buffer=context().buffer()]
                                        (size_t index, Selection& sel) {
            Kakoune::insert(buffer, sel, sel.cursor(), strings[std::min(strings.size()-1, index)]);
        });
    }

    void insert(Codepoint key)
    {
        String str{key};
        insert(str);
        context().hooks().run_hook(Hook::InsertChar, str, context());
    }

    void prepare(InsertMode mode, int count)
    {
        SelectionList& selections = context().selections();
        Buffer& buffer = context().buffer();

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
                sel.set(sel.min(),  buffer.char_next(sel.max()));
                if (sel.cursor() == buffer.end_coord())
                    buffer.insert(buffer.end_coord(), "\n");
            }
            break;
        case InsertMode::AppendAtLineEnd:
            for (auto& sel : selections)
                sel.set({sel.max().line, buffer[sel.max().line].length() - 1});
            break;
        case InsertMode::OpenLineBelow:
        {
            Vector<Selection> new_sels;
            count = count > 0 ? count : 1;
            LineCount inserted_count = 0;
            for (auto sel : selections)
            {
                buffer.insert(sel.max().line + inserted_count + 1,
                              String{'\n', CharCount{count}});
                for (int i = 0; i < count; ++i)
                    new_sels.push_back({sel.max().line + inserted_count + i + 1});
                inserted_count += count;
            }
            selections.set(std::move(new_sels),
                           selections.main_index() * count + count - 1);
            context().hooks().run_hook(Hook::InsertChar, "\n", context());
            break;
        }
        case InsertMode::OpenLineAbove:
        {
            Vector<Selection> new_sels;
            count = count > 0 ? count : 1;
            LineCount inserted_count = 0;
            for (auto sel : selections)
            {
                buffer.insert(sel.min().line + inserted_count,
                              String{'\n', CharCount{count}});
                for (int i = 0; i < count; ++i)
                    new_sels.push_back({sel.min().line + inserted_count + i});
                inserted_count += count;
            }
            selections.set(std::move(new_sels),
                           selections.main_index() * count + count - 1);
            context().hooks().run_hook(Hook::InsertChar, "\n", context());
            break;
        }
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
        }
        selections.check_invariant();
        buffer.check_invariant();
    }

    ScopedEdition   m_edition;
    InsertCompleter m_completer;
    const bool      m_restore_cursor;
    bool            m_auto_complete;
    Timer           m_idle_timer;
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

    context().hooks().run_hook(Hook::ModeChange, format("push:{}:{}", prev_name, new_mode->name()), context());
}

void InputHandler::pop_mode(InputMode* mode)
{
    kak_assert(m_mode_stack.back().get() == mode);
    kak_assert(m_mode_stack.size() > 1);

    RefPtr<InputMode> keep_alive{mode}; // Ensure prev_name stays valid
    StringView prev_name = mode->name();

    current_mode().on_disabled(false);
    m_mode_stack.pop_back();
    current_mode().on_enabled();

    context().hooks().run_hook(Hook::ModeChange, format("pop:{}:{}", prev_name, current_mode().name()), context());
}

void InputHandler::reset_normal_mode()
{
    kak_assert(dynamic_cast<InputModes::Normal*>(m_mode_stack[0].get()) != nullptr);
    if (m_mode_stack.size() == 1)
        return;

    while (m_mode_stack.size() > 1)
        pop_mode(m_mode_stack.back().get());
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
        // refill last_insert,  this is very inefficient, but necessary at the moment
        // to properly handle insert completion
        m_last_insert.keys.push_back(key);
        current_mode().handle_key(key);
    }
    kak_assert(dynamic_cast<InputModes::Normal*>(&current_mode()) != nullptr);
}

void InputHandler::prompt(StringView prompt, String initstr, String emptystr,
                          Face prompt_face, PromptFlags flags, char history_register,
                          PromptCompleter completer, PromptCallback callback)
{
    push_mode(new InputModes::Prompt(*this, prompt, std::move(initstr), std::move(emptystr),
                                     prompt_face, flags, history_register,
                                     std::move(completer), std::move(callback)));
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

void InputHandler::on_next_key(StringView mode_name, KeymapMode keymap_mode, KeyCallback callback,
                               Timer::Callback idle_callback)
{
    push_mode(new InputModes::NextKey(*this, format("next-key[{}]", mode_name), keymap_mode, std::move(callback),
                                      std::move(idle_callback)));
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

    if (m_mode == m_handler.m_mode_stack.back().get())
        m_handler.pop_mode(m_mode);
    else if (auto it = find(m_handler.m_mode_stack, m_mode);
             it != m_handler.m_mode_stack.end())
        m_handler.m_mode_stack.erase(it);
}

static bool is_valid(Key key)
{
    constexpr Key::Modifiers valid_mods = (Key::Modifiers::Control | Key::Modifiers::Alt | Key::Modifiers::Shift);

    return key != Key::Invalid and
        ((key.modifiers & ~valid_mods) or key.key <= 0x10FFFF);
}

void InputHandler::handle_key(Key key)
{
    if (not is_valid(key))
        return;

    const bool was_recording = is_recording();
    ++m_handle_key_level;
    auto dec = on_scope_end([this]{ --m_handle_key_level; });

    auto process_key = [&](Key key) {
        if (m_last_insert.recording)
            m_last_insert.keys.push_back(key);
        current_mode().handle_key(key);
    };

    const auto keymap_mode = current_mode().keymap_mode();
    KeymapManager& keymaps = m_context.keymaps();
    if (keymaps.is_mapped(key, keymap_mode) and not m_context.keymaps_disabled())
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
        m_recorded_keys += to_string(key);

    if (m_handle_key_level < m_recording_level)
    {
        write_to_debug_buffer("Macro recording started but not finished");
        m_recording_reg = 0;
        m_recording_level = -1;
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

bool should_show_info(AutoInfo mask, const Context& context)
{
  return (context.options()["autoinfo"].get<AutoInfo>() & mask) and context.has_client();
}

bool show_auto_info_ifn(StringView title, StringView info, AutoInfo mask, const Context& context)
{
    if (not should_show_info(mask, context))
        return false;

    context.client().info_show(title.str(), info.str(), {}, InfoStyle::Prompt);
    return true;
}

void hide_auto_info_ifn(const Context& context, bool hide)
{
    if (hide)
        context.client().info_hide();
}

void scroll_window(Context& context, LineCount offset, bool mouse_dragging)
{
    Window& window = context.window();
    Buffer& buffer = context.buffer();
    const LineCount line_count = buffer.line_count();

    DisplayCoord win_pos = window.position();
    DisplayCoord win_dim = window.dimensions();

    if ((offset < 0 and win_pos.line == 0) or (offset > 0 and win_pos.line == line_count - 1))
        return;

    const DisplayCoord max_offset{(win_dim.line - 1)/2, (win_dim.column - 1)/2};
    const DisplayCoord scrolloff =
        std::min(context.options()["scrolloff"].get<DisplayCoord>(), max_offset);

    win_pos.line = clamp(win_pos.line + offset, 0_line, line_count-1);

    SelectionList& selections = context.selections();
    Selection& main_selection = selections.main();
    const BufferCoord anchor = main_selection.anchor();
    const BufferCoord cursor = main_selection.cursor();

    auto cursor_off = mouse_dragging ? win_pos.line - window.position().line : 0;

    auto line = clamp(cursor.line + cursor_off, win_pos.line + scrolloff.line,
                      win_pos.line + win_dim.line - 1 - scrolloff.line);
    line = clamp(line, 0_line, buffer.line_count() - 1);

    using std::min; using std::max;
    // This is not exactly a clamp, and must be done in this order as
    // byte_count_to could return line length
    auto col = min(max(cursor.column, buffer[line].byte_count_to(win_pos.column)),
                   buffer[line].length()-1);

    BufferCoord new_cursor = { line, col };
    BufferCoord new_anchor = (mouse_dragging or new_cursor == cursor) ? anchor : new_cursor;

    window.set_position(win_pos);
    main_selection = { new_anchor, new_cursor };

    selections.sort_and_merge_overlapping();
}

}
