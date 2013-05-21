#include "normal.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "client_manager.hh"
#include "color_registry.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "file.hh"
#include "option_manager.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "window.hh"
#include "user_interface.hh"

namespace Kakoune
{

using namespace std::placeholders;

template<InsertMode mode>
void insert(Context& context)
{
    context.input_handler().insert(mode);
}

void repeat_insert(Context& context)
{
    context.input_handler().repeat_last_insert();
}

bool show_auto_info_ifn(const String& info, const Context& context)
{
    if (not context.options()["autoinfo"].get<bool>() or not context.has_ui())
        return false;
    ColorPair col{ Colors::Black, Colors::Yellow };
    DisplayCoord pos = context.window().dimensions();
    pos.column -= 1;
    context.ui().info_show(info, pos , col, MenuStyle::Inline);
    return true;
}

template<SelectMode mode>
void goto_commands(Context& context)
{
    int count = context.numeric_param();
    if (count != 0)
    {
        BufferIterator target =
            context.editor().buffer().iterator_at_line_begin(count-1);

        context.push_jump();
        context.editor().select(target);
        if (context.has_window())
            context.window().center_selection();
    }
    else
    {
        const bool hide = show_auto_info_ifn("╭────────┤goto├───────╮\n"
                                             "│ g,k:  buffer top    │\n"
                                             "│ l:    line end      │\n"
                                             "│ h:    line begin    │\n"
                                             "│ j:    buffer bottom │\n"
                                             "│ e:    buffer end    │\n"
                                             "│ t:    window top    │\n"
                                             "│ b:    window bottom │\n"
                                             "│ c:    window center │\n"
                                             "│ a:    last buffer   │\n"
                                             "│ f:    file          │\n"
                                             "╰─────────────────────╯\n", context);
        context.input_handler().on_next_key([=](const Key& key, Context& context) {
            if (hide)
                context.ui().info_hide();
            if (key.modifiers != Key::Modifiers::None)
                return;

            Editor& editor = context.editor();
            switch (tolower(key.key))
            {
            case 'g':
            case 'k':
                context.push_jump();
                editor.select(editor.buffer().begin(), mode);
                break;
            case 'l':
                editor.select(select_to_eol, mode);
                break;
            case 'h':
                editor.select(select_to_eol_reverse, mode);
                break;
            case 'j':
            {
                context.push_jump();
                const Buffer& buf = editor.buffer();
                editor.select(buf.iterator_at_line_begin(buf.line_count() - 1), mode);
                break;
            }
            case 'e':
                context.push_jump();
                editor.select(editor.buffer().end()-1, mode);
                break;
            case 't':
                if (context.has_window())
                {
                    auto line = context.window().position().line;
                    editor.select(editor.buffer().iterator_at_line_begin(line), mode);
                }
                break;
            case 'b':
                if (context.has_window())
                {
                    auto& window = context.window();
                    auto line = window.position().line + window.dimensions().line - 1;
                    editor.select(editor.buffer().iterator_at_line_begin(line), mode);
                }
                break;
            case 'c':
                if (context.has_window())
                {
                    auto& window = context.window();
                    auto line = window.position().line + window.dimensions().line / 2;
                    editor.select(editor.buffer().iterator_at_line_begin(line), mode);
                }
                break;
            case 'a':
            {
                auto& buffer_manager = BufferManager::instance();
                auto it = buffer_manager.begin();
                if (it->get() == &context.buffer() and ++it == buffer_manager.end())
                    break;
                context.push_jump();
                auto& client_manager = ClientManager::instance();
                context.change_editor(client_manager.get_unused_window_for_buffer(**it));
                break;
            }
            case 'f':
            {
                String filename = context.editor().main_selection().content();
                static constexpr char forbidden[] = { '\'', '\\', '\0' };
                for (auto c : forbidden)
                    if (contains(filename, c))
                        return;

                auto paths = context.options()["path"].get<std::vector<String>>();
                const String& buffer_name = context.buffer().name();
                auto it = find(reversed(buffer_name), '/');
                if (it != buffer_name.rend())
                    paths.insert(paths.begin(), String{buffer_name.begin(), it.base()});

                String path = find_file(filename, paths);
                if (path.empty())
                    throw runtime_error("unable to find file '" + filename + "'");
                CommandManager::instance().execute("edit '" + path + "'", context);
                break;
            }
            }
        });
    }
}

void view_commands(Context& context)
{
    const bool hide = show_auto_info_ifn("╭─────────┤view├─────────╮\n"
                                         "│ v,c:  center cursor    │\n"
                                         "│ t:    cursor on top    │\n"
                                         "│ b:    cursor on bottom │\n"
                                         "│ j:    scroll down      │\n"
                                         "│ k:    scroll up        │\n"
                                         "╰────────────────────────╯\n", context);
    context.input_handler().on_next_key([hide](const Key& key, Context& context) {
        if (hide)
            context.ui().info_hide();
        if (key.modifiers != Key::Modifiers::None or not context.has_window())
            return;

        Window& window = context.window();
        switch (tolower(key.key))
        {
        case 'v':
        case 'c':
            context.window().center_selection();
            break;
        case 't':
            context.window().display_selection_at(0);
            break;
        case 'b':
            context.window().display_selection_at(window.dimensions().line-1);
            break;
        case 'j':
            context.window().scroll( std::max<LineCount>(1, context.numeric_param()));
            break;
        case 'k':
            context.window().scroll(-std::max<LineCount>(1, context.numeric_param()));
            break;
        }
    });
}

void replace_with_char(Context& context)
{
    context.input_handler().on_next_key([](const Key& key, Context& context) {
        Editor& editor = context.editor();
        SelectionList sels = editor.selections();
        auto restore_sels = on_scope_end([&]{ editor.select(std::move(sels)); });
        editor.multi_select(std::bind(select_all_matches, _1, Regex{"."}));
        editor.insert(codepoint_to_str(key.key), InsertMode::Replace);
    });
}

Codepoint swapped_case(Codepoint cp)
{
    Codepoint res = std::tolower(cp);
    return res == cp ? std::toupper(cp) : res;
}

void swap_case(Context& context)
{
    Editor& editor = context.editor();
    std::vector<String> sels = editor.selections_content();
    for (auto& sel : sels)
    {
        for (auto& c : sel)
            c = swapped_case(c);
    }
    editor.insert(sels, InsertMode::Replace);
}

void command(Context& context)
{
    context.input_handler().prompt(
        ":", get_color("Prompt"),
        std::bind(&CommandManager::complete, &CommandManager::instance(), _1, _2, _3),
        [](const String& cmdline, PromptEvent event, Context& context) {
             if (event == PromptEvent::Validate)
                 CommandManager::instance().execute(cmdline, context);
        });
}

void pipe(Context& context)
{
    context.input_handler().prompt("pipe:", get_color("Prompt"), complete_nothing,
        [](const String& cmdline, PromptEvent event, Context& context)
        {
            if (event != PromptEvent::Validate)
                return;

            Editor& editor = context.editor();
            std::vector<String> strings;
            for (auto& sel : context.editor().selections())
                strings.push_back(ShellManager::instance().pipe({sel.begin(), sel.end()},
                                                                cmdline, context, {},
                                                                EnvVarMap{}));
            editor.insert(strings, InsertMode::Replace);
        });
}

template<SelectMode mode, bool forward>
void search(Context& context)
{
    const char* prompt = forward ? "search:" : "reverse search:";
    DynamicSelectionList selections{context.buffer(), context.editor().selections()};
    context.input_handler().prompt(prompt, get_color("Prompt"), complete_nothing,
        [selections](const String& str, PromptEvent event, Context& context) {
            try
            {
                context.editor().select(selections);

                if (event == PromptEvent::Abort)
                    return;

                Regex ex{str};
                context.input_handler().set_prompt_colors(get_color("Prompt"));
                if (event == PromptEvent::Validate)
                {
                    if (str.empty())
                        ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
                    else
                        RegisterManager::instance()['/'] = str;
                    context.push_jump();
                }
                else if (str.empty() or not context.options()["incsearch"].get<bool>())
                    return;

                context.editor().select(std::bind(select_next_match<forward>, _1, ex), mode);
            }
            catch (boost::regex_error& err)
            {
                if (event == PromptEvent::Validate)
                    throw runtime_error("regex error: "_str + err.what());
                else
                    context.input_handler().set_prompt_colors(get_color("Error"));
            }
            catch (runtime_error&)
            {
                context.editor().select(selections);
                // only validation should propagate errors,
                // incremental search should not.
                if (event == PromptEvent::Validate)
                    throw;
            }
        });
}

template<SelectMode mode, bool forward>
void search_next(Context& context)
{
    const String& str = RegisterManager::instance()['/'].values(context)[0];
    if (not str.empty())
    {
        try
        {
            Regex ex{str};
            if (mode == SelectMode::Replace)
                context.push_jump();
            int count = context.numeric_param();
            do {
                context.editor().select(std::bind(select_next_match<forward>, _1, ex), mode);
            } while (--count > 0);
        }
        catch (boost::regex_error& err)
        {
            throw runtime_error("regex error: "_str + err.what());
        }
    }
    else
        throw runtime_error("no search pattern");
}

template<bool smart>
void use_selection_as_search_pattern(Context& context)
{
    std::vector<String> patterns;
    auto& sels = context.editor().selections();
    for (auto& sel : sels)
    {
        auto begin = sel.begin();
        auto end = sel.end();
        auto content = "\\Q" + context.buffer().string(begin, end) + "\\E";
        if (smart)
        {
            if (begin.is_begin() or
                (is_word(utf8::codepoint(begin)) and not
                 is_word(utf8::codepoint(utf8::previous(begin)))))
                content = "\\b" + content;
            if (end.is_end() or
                (is_word(utf8::codepoint(utf8::previous(end))) and not
                 is_word(utf8::codepoint(end))))
                content = content + "\\b";
        }
        patterns.push_back(std::move(content));
    }
    RegisterManager::instance()['/'] = patterns;
}

void yank(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.print_status({ "yanked " + to_string(context.editor().selections().size()) +
                           " selections", get_color("Information") });
}

void cat_yank(Context& context)
{
    auto sels = context.editor().selections_content();
    String str;
    for (auto& sel : sels)
        str += sel;
    RegisterManager::instance()['"'] = memoryview<String>(str);
    context.print_status({ "concatenated and yanked " +
                           to_string(sels.size()) + " selections", get_color("Information") });
}

void erase(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.editor().erase();
}

void change(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    insert<InsertMode::Replace>(context);
}

static InsertMode adapt_for_linewise(InsertMode mode)
{
    if (mode == InsertMode::Append)
        return InsertMode::InsertAtNextLineBegin;
    if (mode == InsertMode::Insert)
        return InsertMode::InsertAtLineBegin;
    if (mode == InsertMode::Replace)
        return InsertMode::Replace;

    kak_assert(false);
    return InsertMode::Insert;
}

template<InsertMode insert_mode>
void paste(Context& context)
{
    Editor& editor = context.editor();
    auto strings = RegisterManager::instance()['"'].values(context);
    InsertMode mode = insert_mode;
    for (auto& str : strings)
    {
        if (not str.empty() and str.back() == '\n')
        {
            mode = adapt_for_linewise(mode);
            break;
        }
    }
    editor.insert(strings, mode);
}

template<typename T>
void regex_prompt(Context& context, const String prompt, T on_validate)
{
    context.input_handler().prompt(prompt, get_color("Prompt"), complete_nothing,
        [=](const String& str, PromptEvent event, Context& context) {
            if (event == PromptEvent::Validate)
            {
                try
                {
                    on_validate(str.empty() ? Regex{} : Regex{str}, context);
                }
                catch (boost::regex_error& err)
                {
                    throw runtime_error("regex error: "_str + err.what());
                }
            }
            else if (event == PromptEvent::Change)
            {
                const bool ok = Regex{str, boost::regex_constants::no_except}.status() == 0;
                context.input_handler().set_prompt_colors(get_color(ok ? "Prompt" : "Error"));
            }
        });
}

void select_regex(Context& context)
{
    regex_prompt(context, "select:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty() and not ex.str().empty())
            context.editor().multi_select(std::bind(select_all_matches, _1, ex));
    });
}

void split_regex(Context& context)
{
    regex_prompt(context, "split:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty() and not ex.str().empty())
            context.editor().multi_select(std::bind(split_selection, _1, ex));
    });
}

void split_lines(Context& context)
{
    context.editor().multi_select(std::bind(split_selection, _1, Regex{"^"}));
}

void join_select_spaces(Context& context)
{
    Editor& editor = context.editor();
    editor.select(select_whole_lines);
    editor.select(select_to_eol, SelectMode::Extend);
    editor.multi_select([](const Selection& sel)
    {
        SelectionList res = select_all_matches(sel, Regex{"(\n\\h*)+"});
        // remove last end of line if selected
        kak_assert(std::is_sorted(res.begin(), res.end(),
              [](const Selection& lhs, const Selection& rhs)
              { return lhs.begin() < rhs.begin(); }));
        if (not res.empty() and res.back().end() == sel.buffer().end())
            res.pop_back();
        return res;
    });
    editor.insert(" ", InsertMode::Replace);
}

void join(Context& context)
{
    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    join_select_spaces(context);
}

template<bool matching>
void keep(Context& context)
{
    constexpr const char* prompt = matching ? "keep matching:" : "keep not matching:";
    regex_prompt(context, prompt, [](const Regex& ex, Context& context) {
        Editor& editor = context.editor();
        SelectionList sels = editor.selections();
        SelectionList keep;
        for (auto& sel : sels)
        {
            if (boost::regex_search(sel.begin(), sel.end(), ex) == matching)
                keep.push_back(sel);
        }
        if (keep.empty())
            throw runtime_error("no selections remaining");
        editor.select(std::move(keep));
    });
}

void indent(Context& context)
{
    size_t width = context.options()["indentwidth"].get<int>();
    String indent(' ', width);

    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    editor.select(select_whole_lines);
    editor.multi_select(std::bind(select_all_matches, _1, Regex{"^[^\n]"}));
    editor.insert(indent, InsertMode::Insert);
}

void deindent(Context& context)
{
    int width = context.options()["indentwidth"].get<int>();
    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    editor.select(select_whole_lines);
    editor.multi_select(std::bind(select_all_matches, _1,
                                  Regex{"^\\h{1," + to_string(width) + "}"}));
    editor.erase();
}

template<ObjectFlags flags>
void select_object(Context& context)
{
    const bool hide = show_auto_info_ifn("╭──────┤select object├───────╮\n"
                                         "│ b,(,):  parenthesis block  │\n"
                                         "│ B,{,}:  braces block       │\n"
                                         "│ [,]:    brackets block     │\n"
                                         "│ <,>:    angle block        │\n"
                                         "│ \":    double quote string  │\n"
                                         "│ ':    single quote string  │\n"
                                         "│ w:    word                 │\n"
                                         "│ W:    WORD                 │\n"
                                         "│ s:    sentence             │\n"
                                         "│ p:    paragraph            │\n"
                                         "╰────────────────────────────╯\n", context);
    context.input_handler().on_next_key(
    [=](const Key& key, Context& context) {
        if (hide)
            context.ui().info_hide();
        typedef std::function<Selection (const Selection&)> Selector;
        static const std::unordered_map<Key, Selector> key_to_selector =
        {
            { { Key::Modifiers::None, '(' }, std::bind(select_surrounding, _1, CodepointPair{ '(', ')' }, flags) },
            { { Key::Modifiers::None, ')' }, std::bind(select_surrounding, _1, CodepointPair{ '(', ')' }, flags) },
            { { Key::Modifiers::None, 'b' }, std::bind(select_surrounding, _1, CodepointPair{ '(', ')' }, flags) },
            { { Key::Modifiers::None, '{' }, std::bind(select_surrounding, _1, CodepointPair{ '{', '}' }, flags) },
            { { Key::Modifiers::None, '}' }, std::bind(select_surrounding, _1, CodepointPair{ '{', '}' }, flags) },
            { { Key::Modifiers::None, 'B' }, std::bind(select_surrounding, _1, CodepointPair{ '{', '}' }, flags) },
            { { Key::Modifiers::None, '[' }, std::bind(select_surrounding, _1, CodepointPair{ '[', ']' }, flags) },
            { { Key::Modifiers::None, ']' }, std::bind(select_surrounding, _1, CodepointPair{ '[', ']' }, flags) },
            { { Key::Modifiers::None, 'r' }, std::bind(select_surrounding, _1, CodepointPair{ '[', ']' }, flags) },
            { { Key::Modifiers::None, '<' }, std::bind(select_surrounding, _1, CodepointPair{ '<', '>' }, flags) },
            { { Key::Modifiers::None, '>' }, std::bind(select_surrounding, _1, CodepointPair{ '<', '>' }, flags) },
            { { Key::Modifiers::None, '"' }, std::bind(select_surrounding, _1, CodepointPair{ '"', '"' }, flags) },
            { { Key::Modifiers::None, '\'' }, std::bind(select_surrounding, _1, CodepointPair{ '\'', '\'' }, flags) },
            { { Key::Modifiers::None, 'w' }, std::bind(select_whole_word<false>, _1, flags) },
            { { Key::Modifiers::None, 'W' }, std::bind(select_whole_word<true>, _1, flags) },
            { { Key::Modifiers::None, 's' }, std::bind(select_whole_sentence, _1, flags) },
            { { Key::Modifiers::None, 'p' }, std::bind(select_whole_paragraph, _1, flags) },
        };

        auto it = key_to_selector.find(key);
        if (it != key_to_selector.end())
            context.editor().select(it->second);
    });
}

template<Key::NamedKey key>
void scroll(Context& context)
{
    static_assert(key == Key::PageUp or key == Key::PageDown,
                  "scrool only implements PageUp and PageDown");
    Window& window = context.window();
    Buffer& buffer = context.buffer();
    DisplayCoord position = window.position();
    LineCount cursor_line = 0;

    if (key == Key::PageUp)
    {
        position.line -= (window.dimensions().line - 2);
        cursor_line = position.line;
    }
    else if (key == Key::PageDown)
    {
        position.line += (window.dimensions().line - 2);
        cursor_line = position.line + window.dimensions().line - 1;
    }
    auto cursor_pos = utf8::advance(buffer.iterator_at_line_begin(position.line),
                                    buffer.iterator_at_line_end(position.line),
                                    position.column);
    window.select(cursor_pos);
    window.set_position(position);
}

void rotate_selections(Context& context)
{
    int count = context.numeric_param();
    if (count == 0)
        count = 1;
    context.editor().rotate_selections(count);
}

enum class SelectFlags
{
    None = 0,
    Reverse = 1,
    Inclusive = 2,
    Extend = 4
};
constexpr SelectFlags operator|(SelectFlags lhs, SelectFlags rhs)
{
    return (SelectFlags)((int) lhs | (int) rhs);
}
constexpr bool operator&(SelectFlags lhs, SelectFlags rhs)
{
    return ((int) lhs & (int) rhs) != 0;
}

template<SelectFlags flags>
void select_to_next_char(Context& context)
{
    int param = context.numeric_param();
    context.input_handler().on_next_key([param](const Key& key, Context& context) {
        context.editor().select(
            std::bind(flags & SelectFlags::Reverse ? select_to_reverse : select_to,
                      _1, key.key, param, flags & SelectFlags::Inclusive),
            flags & SelectFlags::Extend ? SelectMode::Extend : SelectMode::Replace);
   });
}

void start_or_end_macro_recording(Context& context)
{
    if (context.input_handler().is_recording())
        context.input_handler().stop_recording();
    else
        context.input_handler().on_next_key([](const Key& key, Context& context) {
            if (key.modifiers == Key::Modifiers::None)
                context.input_handler().start_recording(key.key);
        });
}

void replay_macro(Context& context)
{
    int count = context.numeric_param();
    context.input_handler().on_next_key([count](const Key& key, Context& context) mutable {
        if (key.modifiers == Key::Modifiers::None)
        {
            static std::unordered_set<char> running_macros;
            if (contains(running_macros, key.key))
                throw runtime_error("recursive macros call detected");

            memoryview<String> reg_val = RegisterManager::instance()[key.key].values(context);
            if (not reg_val.empty())
            {
                running_macros.insert(key.key);
                auto stop = on_scope_end([&]{ running_macros.erase(key.key); });

                auto keys = parse_keys(reg_val[0]);
                scoped_edition edition(context.editor());
                do { exec_keys(keys, context); } while (--count > 0);
            }
        }
    });
}

enum class JumpDirection { Forward, Backward };
template<JumpDirection direction>
void jump(Context& context)
{
    auto jump = (direction == JumpDirection::Forward) ?
                 context.jump_forward() : context.jump_backward();

    Buffer& buffer = const_cast<Buffer&>(jump.front().buffer());
    BufferManager::instance().set_last_used_buffer(buffer);
    if (&buffer != &context.buffer())
    {
        auto& manager = ClientManager::instance();
        context.change_editor(manager.get_unused_window_for_buffer(buffer));
    }
    context.editor().select(SelectionList{ jump });
}

void align(Context& context)
{
    auto& selections = context.editor().selections();
    auto& buffer = context.buffer();
    auto get_column = [&buffer](const BufferIterator& it)
    { return utf8::distance(buffer.iterator_at_line_begin(it), it); };

    CharCount max_col = 0;
    for (auto& sel : selections)
        max_col = std::max(get_column(sel.last()), max_col);

    for (auto& sel : selections)
    {
        CharCount padding = max_col - get_column(sel.last());
        buffer.insert(sel.last(), String{ ' ', padding });
    }
}

template<typename T>
class Repeated
{
public:
    constexpr Repeated(T t) : m_func(t) {}

    void operator() (Context& context)
    {
        scoped_edition edition(context.editor());
        int count = context.numeric_param();
        do { m_func(context); } while(--count > 0);
    }
private:
    T m_func;
};

template<typename T>
constexpr Repeated<T> repeated(T func) { return Repeated<T>(func); }

template<SelectMode mode, typename T>
class Select
{
public:
    constexpr Select(T t) : m_func(t) {}

    void operator() (Context& context)
    {
        context.editor().select(m_func, mode);
    }
private:
    T m_func;
};

template<SelectMode mode, typename T>
constexpr Select<mode, T> select(T func) { return Select<mode, T>(func); }

KeyMap keymap =
{
    { { Key::Modifiers::None, 'h' }, [](Context& context) { context.editor().move_selections(-CharCount(std::max(context.numeric_param(),1))); } },
    { { Key::Modifiers::None, 'j' }, [](Context& context) { context.editor().move_selections( LineCount(std::max(context.numeric_param(),1))); } },
    { { Key::Modifiers::None, 'k' }, [](Context& context) { context.editor().move_selections(-LineCount(std::max(context.numeric_param(),1))); } },
    { { Key::Modifiers::None, 'l' }, [](Context& context) { context.editor().move_selections( CharCount(std::max(context.numeric_param(),1))); } },

    { { Key::Modifiers::None, 'H' }, [](Context& context) { context.editor().move_selections(-CharCount(std::max(context.numeric_param(),1)), SelectMode::Extend); } },
    { { Key::Modifiers::None, 'J' }, [](Context& context) { context.editor().move_selections( LineCount(std::max(context.numeric_param(),1)), SelectMode::Extend); } },
    { { Key::Modifiers::None, 'K' }, [](Context& context) { context.editor().move_selections(-LineCount(std::max(context.numeric_param(),1)), SelectMode::Extend); } },
    { { Key::Modifiers::None, 'L' }, [](Context& context) { context.editor().move_selections( CharCount(std::max(context.numeric_param(),1)), SelectMode::Extend); } },

    { { Key::Modifiers::None, 't' }, select_to_next_char<SelectFlags::None> },
    { { Key::Modifiers::None, 'f' }, select_to_next_char<SelectFlags::Inclusive> },
    { { Key::Modifiers::None, 'T' }, select_to_next_char<SelectFlags::Extend> },
    { { Key::Modifiers::None, 'F' }, select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend> },
    { { Key::Modifiers::Alt,  't' }, select_to_next_char<SelectFlags::Reverse> },
    { { Key::Modifiers::Alt,  'f' }, select_to_next_char<SelectFlags::Inclusive | SelectFlags::Reverse> },
    { { Key::Modifiers::Alt,  'T' }, select_to_next_char<SelectFlags::Extend | SelectFlags::Reverse> },
    { { Key::Modifiers::Alt,  'F' }, select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend | SelectFlags::Reverse> },

    { { Key::Modifiers::None, 'd' }, erase },
    { { Key::Modifiers::None, 'c' }, change },
    { { Key::Modifiers::None, 'i' }, insert<InsertMode::Insert> },
    { { Key::Modifiers::None, 'I' }, insert<InsertMode::InsertAtLineBegin> },
    { { Key::Modifiers::None, 'a' }, insert<InsertMode::Append> },
    { { Key::Modifiers::None, 'A' }, insert<InsertMode::AppendAtLineEnd> },
    { { Key::Modifiers::None, 'o' }, insert<InsertMode::OpenLineBelow> },
    { { Key::Modifiers::None, 'O' }, insert<InsertMode::OpenLineAbove> },
    { { Key::Modifiers::None, 'r' }, replace_with_char },

    { { Key::Modifiers::None, 'g' }, goto_commands<SelectMode::Replace> },
    { { Key::Modifiers::None, 'G' }, goto_commands<SelectMode::Extend> },

    { { Key::Modifiers::None, 'v' }, view_commands },

    { { Key::Modifiers::None, 'y' }, yank },
    { { Key::Modifiers::None, 'Y' }, cat_yank },
    { { Key::Modifiers::None, 'p' }, repeated(paste<InsertMode::Append>) },
    { { Key::Modifiers::None, 'P' }, repeated(paste<InsertMode::Insert>) },
    { { Key::Modifiers::Alt,  'p' }, paste<InsertMode::Replace> },

    { { Key::Modifiers::None, 's' }, select_regex },
    { { Key::Modifiers::None, 'S' }, split_regex },
    { { Key::Modifiers::Alt,  's' }, split_lines },

    { { Key::Modifiers::None, '.' }, repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Context& context) { context.editor().clear_selections(); context.editor().select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, command },
    { { Key::Modifiers::None, '|' }, pipe },
    { { Key::Modifiers::None, ' ' }, [](Context& context) { int count = context.numeric_param();
                                                            if (count == 0) context.editor().clear_selections();
                                                            else context.editor().keep_selection(count-1); } },
    { { Key::Modifiers::Alt,  ' ' }, [](Context& context) { int count = context.numeric_param();
                                                            if (count == 0) context.editor().flip_selections();
                                                            else context.editor().remove_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, repeated(select<SelectMode::Replace>(select_to_next_word<false>)) },
    { { Key::Modifiers::None, 'e' }, repeated(select<SelectMode::Replace>(select_to_next_word_end<false>)) },
    { { Key::Modifiers::None, 'b' }, repeated(select<SelectMode::Replace>(select_to_previous_word<false>)) },
    { { Key::Modifiers::None, 'W' }, repeated(select<SelectMode::Extend>(select_to_next_word<false>)) },
    { { Key::Modifiers::None, 'E' }, repeated(select<SelectMode::Extend>(select_to_next_word_end<false>)) },
    { { Key::Modifiers::None, 'B' }, repeated(select<SelectMode::Extend>(select_to_previous_word<false>)) },

    { { Key::Modifiers::Alt,  'w' }, repeated(select<SelectMode::Replace>(select_to_next_word<true>)) },
    { { Key::Modifiers::Alt,  'e' }, repeated(select<SelectMode::Replace>(select_to_next_word_end<true>)) },
    { { Key::Modifiers::Alt,  'b' }, repeated(select<SelectMode::Replace>(select_to_previous_word<true>)) },
    { { Key::Modifiers::Alt,  'W' }, repeated(select<SelectMode::Extend>(select_to_next_word<true>)) },
    { { Key::Modifiers::Alt,  'E' }, repeated(select<SelectMode::Extend>(select_to_next_word_end<true>)) },
    { { Key::Modifiers::Alt,  'B' }, repeated(select<SelectMode::Extend>(select_to_previous_word<true>)) },

    { { Key::Modifiers::Alt,  'l' }, repeated(select<SelectMode::Replace>(select_to_eol)) },
    { { Key::Modifiers::Alt,  'L' }, repeated(select<SelectMode::Extend>(select_to_eol)) },
    { { Key::Modifiers::Alt,  'h' }, repeated(select<SelectMode::Replace>(select_to_eol_reverse)) },
    { { Key::Modifiers::Alt,  'H' }, repeated(select<SelectMode::Extend>(select_to_eol_reverse)) },

    { { Key::Modifiers::None, 'x' }, repeated(select<SelectMode::Replace>(select_line)) },
    { { Key::Modifiers::None, 'X' }, repeated(select<SelectMode::Extend>(select_line)) },
    { { Key::Modifiers::Alt,  'x' }, select<SelectMode::Replace>(select_whole_lines) },
    { { Key::Modifiers::Alt,  'X' }, select<SelectMode::Replace>(trim_partial_lines) },

    { { Key::Modifiers::None, 'm' }, select<SelectMode::Replace>(select_matching) },
    { { Key::Modifiers::None, 'M' }, select<SelectMode::Extend>(select_matching) },

    { { Key::Modifiers::None, '/' }, search<SelectMode::Replace, true> },
    { { Key::Modifiers::None, '?' }, search<SelectMode::Extend, true> },
    { { Key::Modifiers::Alt,  '/' }, search<SelectMode::Replace, false> },
    { { Key::Modifiers::Alt,  '?' }, search<SelectMode::Extend, false> },
    { { Key::Modifiers::None, 'n' }, search_next<SelectMode::Replace, true> },
    { { Key::Modifiers::Alt,  'n' }, search_next<SelectMode::ReplaceMain, true> },
    { { Key::Modifiers::None, 'N' }, search_next<SelectMode::Append, true> },
    { { Key::Modifiers::None, '*' }, use_selection_as_search_pattern<true> },
    { { Key::Modifiers::Alt,  '*' }, use_selection_as_search_pattern<false> },

    { { Key::Modifiers::None, 'u' }, repeated([](Context& context) { if (not context.editor().undo()) { context.print_status({ "nothing left to undo", get_color("Information") }); } }) },
    { { Key::Modifiers::None, 'U' }, repeated([](Context& context) { if (not context.editor().redo()) { context.print_status({ "nothing left to redo", get_color("Information") }); } }) },

    { { Key::Modifiers::Alt,  'i' }, select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner> },
    { { Key::Modifiers::Alt,  'a' }, select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd> },
    { { Key::Modifiers::None, ']' }, select_object<ObjectFlags::ToEnd> },
    { { Key::Modifiers::None, '[' }, select_object<ObjectFlags::ToBegin> },

    { { Key::Modifiers::Alt,  'j' }, join },
    { { Key::Modifiers::Alt,  'J' }, join_select_spaces },

    { { Key::Modifiers::Alt,  'k' }, keep<true> },
    { { Key::Modifiers::Alt,  'K' }, keep<false> },

    { { Key::Modifiers::None, '<' }, deindent },
    { { Key::Modifiers::None, '>' }, indent },

    { { Key::Modifiers::None, Key::PageUp }, scroll<Key::PageUp> },
    { { Key::Modifiers::None, Key::PageDown }, scroll<Key::PageDown> },

    { { Key::Modifiers::Control, 'i' }, jump<JumpDirection::Forward> },
    { { Key::Modifiers::Control, 'o' }, jump<JumpDirection::Backward> },

    { { Key::Modifiers::Alt,  'r' }, rotate_selections },

    { { Key::Modifiers::None, 'q' }, start_or_end_macro_recording },
    { { Key::Modifiers::None, 'Q' }, replay_macro },

    { { Key::Modifiers::None, '~' }, swap_case },
    { { Key::Modifiers::None, '&' }, align },
};

}
