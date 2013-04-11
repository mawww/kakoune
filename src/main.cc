#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "client_manager.hh"
#include "color_registry.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "file.hh"
#include "filters.hh"
#include "highlighters.hh"
#include "hook_manager.hh"
#include "ncurses.hh"
#include "option_manager.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "selectors.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "window.hh"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <unordered_map>
#include <locale>
#include <signal.h>

using namespace Kakoune;
using namespace std::placeholders;

namespace Kakoune
{

template<InsertMode mode>
void do_insert(Context& context)
{
    context.input_handler().insert(mode);
}

void do_repeat_insert(Context& context)
{
    context.input_handler().repeat_last_insert();
}

template<SelectMode mode>
void do_go(Context& context)
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
        context.input_handler().on_next_key([](const Key& key, Context& context) {
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
                static char forbidden[] = { '\'', '\\', '\0' };
                for (auto c : forbidden)
                    if (contains(filename, c))
                        return;

                auto paths = context.options()["path"].get<std::vector<String>>();
                const String& buffer_name = context.buffer().name();
                auto it = find(reversed(buffer_name), '/');
                if (it != buffer_name.rend())
                    paths.insert(paths.begin(), String{buffer_name.begin(), it.base()});

                String path = find_file(filename, paths);
                if (not path.empty())
                    CommandManager::instance().execute("edit '" + path + "'", context);
                break;
            }
            }
        });
}

void do_disp_cmd(Context& context)
{
    context.input_handler().on_next_key([](const Key& key, Context& context) {
        if (key.modifiers != Key::Modifiers::None or not context.has_window())
            return;

        Window& window = context.window();
        switch (tolower(key.key))
        {
        case 'z':
        case 'c':
            context.window().center_selection();
            break;
        case 't':
            context.window().display_selection_at(0);
            break;
        case 'b':
            context.window().display_selection_at(window.dimensions().line-1);
            break;
        }
    });
}

void do_replace_with_char(Context& context)
{
    context.input_handler().on_next_key([](const Key& key, Context& context) {
        Editor& editor = context.editor();
        SelectionList sels = editor.selections();
        auto restore_sels = on_scope_end([&]{ editor.select(std::move(sels)); });
        editor.multi_select(std::bind(select_all_matches, _1, Regex{"."}));
        editor.insert(codepoint_to_str(key.key), InsertMode::Replace);
    });
}

Codepoint swap_case(Codepoint cp)
{
    if ('A' <= cp and cp <= 'Z')
        return cp - 'A' + 'a';
    if ('a' <= cp and cp <= 'z')
        return cp - 'a' + 'A';
    return cp;
}

void do_swap_case(Context& context)
{
    Editor& editor = context.editor();
    std::vector<String> sels = editor.selections_content();
    for (auto& sel : sels)
    {
        for (auto& c : sel)
            c = swap_case(c);
    }
    editor.insert(sels, InsertMode::Replace);
}

void do_command(Context& context)
{
    context.input_handler().prompt(
        ":", get_color("Prompt"),
        std::bind(&CommandManager::complete, &CommandManager::instance(), _1, _2, _3),
        [](const String& cmdline, PromptEvent event, Context& context) {
             if (event == PromptEvent::Validate)
                 CommandManager::instance().execute(cmdline, context);
        });
}

void do_pipe(Context& context)
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
void do_search(Context& context)
{
    const char* prompt = forward ? "search:" : "reverse search:";
    SelectionList selections = context.editor().selections();
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
void do_search_next(Context& context)
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

void do_yank(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.print_status({ "yanked " + int_to_str(context.editor().selections().size()) + " selections", get_color("Information") });
}

void do_cat_yank(Context& context)
{
    auto sels = context.editor().selections_content();
    String str;
    for (auto& sel : sels)
        str += sel;
    RegisterManager::instance()['"'] = memoryview<String>(str);
    context.print_status({ "concatenated and yanked " +
                           int_to_str(sels.size()) + " selections", get_color("Information") });
}

void do_erase(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.editor().erase();
}

void do_change(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    do_insert<InsertMode::Replace>(context);
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
void do_paste(Context& context)
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
                    on_validate(Regex{str}, context);
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

void do_select_regex(Context& context)
{
    regex_prompt(context, "select:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty())
            context.editor().multi_select(std::bind(select_all_matches, _1, ex));
    });
}

void do_split_regex(Context& context)
{
    regex_prompt(context, "split:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty())
            context.editor().multi_select(std::bind(split_selection, _1, ex));
    });
}

void do_split_lines(Context& context)
{
    context.editor().multi_select(std::bind(split_selection, _1, Regex{"^"}));
}

void do_join(Context& context)
{
    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    editor.select(select_whole_lines);
    editor.select(select_to_eol, SelectMode::Extend);
    editor.multi_select([](const Selection& sel)
    {
        SelectionList res = select_all_matches(sel, Regex{"\n\\h*"});
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

template<bool matching>
void do_keep(Context& context)
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

void do_indent(Context& context)
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

void do_deindent(Context& context)
{
    int width = context.options()["indentwidth"].get<int>();
    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    editor.select(select_whole_lines);
    editor.multi_select(std::bind(select_all_matches, _1,
                                  Regex{"^\\h{1," + int_to_str(width) + "}"}));
    editor.erase();
}

template<SurroundFlags flags>
void do_select_object(Context& context)
{
    context.input_handler().on_next_key(
    [](const Key& key, Context& context) {
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
            { { Key::Modifiers::None, 'w' }, std::bind(select_whole_word<false>, _1, flags & SurroundFlags::Inner) },
            { { Key::Modifiers::None, 'W' }, std::bind(select_whole_word<true>, _1, flags & SurroundFlags::Inner) },
        };

        auto it = key_to_selector.find(key);
        if (it != key_to_selector.end())
            context.editor().select(it->second);
    });
}

template<Key::NamedKey key>
void do_scroll(Context& context)
{
    static_assert(key == Key::PageUp or key == Key::PageDown,
                  "do_scrool only implements PageUp and PageDown");
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

void do_rotate_selections(Context& context)
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
    static bool running_macro = false;
    if (running_macro)
        throw runtime_error("nested macros not supported");

    int count = context.numeric_param();
    context.input_handler().on_next_key([count](const Key& key, Context& context) mutable {
        if (key.modifiers == Key::Modifiers::None)
        {
            memoryview<String> reg_val = RegisterManager::instance()[key.key].values(context);
            if (not reg_val.empty())
            {
                running_macro = true;
                auto stop_macro = on_scope_end([&] { running_macro = false; });

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

String runtime_directory()
{
    char buffer[2048];
#if defined(__linux__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
#elif defined(__APPLE__)
    uint32_t bufsize = 2048;
    _NSGetExecutablePath(buffer, &bufsize);
    char* canonical_path = realpath(buffer, NULL);
    strncpy(buffer, canonical_path, 2048);
    free(canonical_path);
#else
# error "finding executable path is not implemented on this platform"
#endif
    char* ptr = strrchr(buffer, '/');
    if (not ptr)
        throw runtime_error("unable to determine runtime directory");
    return String(buffer, ptr);
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

std::unordered_map<Key, std::function<void (Context& context)>> keymap =
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

    { { Key::Modifiers::None, 'd' }, do_erase },
    { { Key::Modifiers::None, 'c' }, do_change },
    { { Key::Modifiers::None, 'i' }, do_insert<InsertMode::Insert> },
    { { Key::Modifiers::None, 'I' }, do_insert<InsertMode::InsertAtLineBegin> },
    { { Key::Modifiers::None, 'a' }, do_insert<InsertMode::Append> },
    { { Key::Modifiers::None, 'A' }, do_insert<InsertMode::AppendAtLineEnd> },
    { { Key::Modifiers::None, 'o' }, do_insert<InsertMode::OpenLineBelow> },
    { { Key::Modifiers::None, 'O' }, do_insert<InsertMode::OpenLineAbove> },
    { { Key::Modifiers::None, 'r' }, do_replace_with_char },

    { { Key::Modifiers::None, 'g' }, do_go<SelectMode::Replace> },
    { { Key::Modifiers::None, 'G' }, do_go<SelectMode::Extend> },

    { { Key::Modifiers::None, 'z' }, do_disp_cmd },

    { { Key::Modifiers::None, 'y' }, do_yank },
    { { Key::Modifiers::None, 'Y' }, do_cat_yank },
    { { Key::Modifiers::None, 'p' }, repeated(do_paste<InsertMode::Append>) },
    { { Key::Modifiers::None, 'P' }, repeated(do_paste<InsertMode::Insert>) },
    { { Key::Modifiers::Alt,  'p' }, do_paste<InsertMode::Replace> },

    { { Key::Modifiers::None, 's' }, do_select_regex },
    { { Key::Modifiers::None, 'S' }, do_split_regex },
    { { Key::Modifiers::Alt,  's' }, do_split_lines },

    { { Key::Modifiers::None, '.' }, do_repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Context& context) { context.editor().clear_selections(); context.editor().select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, do_command },
    { { Key::Modifiers::None, '|' }, do_pipe },
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

    { { Key::Modifiers::None, 'm' }, select<SelectMode::Replace>(select_matching) },
    { { Key::Modifiers::None, 'M' }, select<SelectMode::Extend>(select_matching) },

    { { Key::Modifiers::None, '/' }, do_search<SelectMode::Replace, true> },
    { { Key::Modifiers::None, '?' }, do_search<SelectMode::Extend, true> },
    { { Key::Modifiers::Alt,  '/' }, do_search<SelectMode::Replace, false> },
    { { Key::Modifiers::Alt,  '?' }, do_search<SelectMode::Extend, false> },
    { { Key::Modifiers::None, 'n' }, do_search_next<SelectMode::Replace, true> },
    { { Key::Modifiers::Alt,  'n' }, do_search_next<SelectMode::ReplaceMain, true> },
    { { Key::Modifiers::None, 'N' }, do_search_next<SelectMode::Append, true> },
    { { Key::Modifiers::None, '*' }, use_selection_as_search_pattern<true> },
    { { Key::Modifiers::Alt,  '*' }, use_selection_as_search_pattern<false> },

    { { Key::Modifiers::None, 'u' }, repeated([](Context& context) { if (not context.editor().undo()) { context.print_status({ "nothing left to undo", get_color("Information") }); } }) },
    { { Key::Modifiers::None, 'U' }, repeated([](Context& context) { if (not context.editor().redo()) { context.print_status({ "nothing left to redo", get_color("Information") }); } }) },

    { { Key::Modifiers::Alt,  'i' }, do_select_object<SurroundFlags::ToBegin | SurroundFlags::ToEnd | SurroundFlags::Inner> },
    { { Key::Modifiers::Alt,  'a' }, do_select_object<SurroundFlags::ToBegin | SurroundFlags::ToEnd> },
    { { Key::Modifiers::None, ']' }, do_select_object<SurroundFlags::ToEnd> },
    { { Key::Modifiers::None, '[' }, do_select_object<SurroundFlags::ToBegin> },

    { { Key::Modifiers::Alt,  'j' }, do_join },

    { { Key::Modifiers::Alt,  'k' }, do_keep<true> },
    { { Key::Modifiers::Alt,  'K' }, do_keep<false> },

    { { Key::Modifiers::None, '<' }, do_deindent },
    { { Key::Modifiers::None, '>' }, do_indent },

    { { Key::Modifiers::None, Key::PageUp }, do_scroll<Key::PageUp> },
    { { Key::Modifiers::None, Key::PageDown }, do_scroll<Key::PageDown> },

    { { Key::Modifiers::Control, 'i' }, jump<JumpDirection::Forward> },
    { { Key::Modifiers::Control, 'o' }, jump<JumpDirection::Backward> },

    { { Key::Modifiers::Alt,  'r' }, do_rotate_selections },

    { { Key::Modifiers::None, 'q' }, start_or_end_macro_recording },
    { { Key::Modifiers::None, 'Q' }, replay_macro },

    { { Key::Modifiers::None, '~' }, do_swap_case },
};

}

void run_unit_tests();

void register_env_vars()
{
    ShellManager& shell_manager = ShellManager::instance();

    shell_manager.register_env_var("bufname",
                                   [](const String& name, const Context& context)
                                   { return context.buffer().display_name(); });
    shell_manager.register_env_var("timestamp",
                                   [](const String& name, const Context& context)
                                   { return int_to_str(context.buffer().timestamp()); });
    shell_manager.register_env_var("selection",
                                   [](const String& name, const Context& context)
                                   { return context.editor().main_selection().content(); });
    shell_manager.register_env_var("selections",
                                   [](const String& name, const Context& context)
                                   { auto sels = context.editor().selections_content();
                                     return std::accumulate(sels.begin(), sels.end(), ""_str,
                                     [](const String& lhs, const String& rhs) { return lhs.empty() ? rhs : lhs + "," + rhs; }); });
    shell_manager.register_env_var("runtime",
                                   [](const String& name, const Context& context)
                                   { return runtime_directory(); });
    shell_manager.register_env_var("opt_.+",
                                   [](const String& name, const Context& context)
                                   { return context.options()[name.substr(4_byte)].get_as_string(); });
    shell_manager.register_env_var("reg_.+",
                                   [](const String& name, const Context& context)
                                   { return RegisterManager::instance()[name[4]].values(context)[0]; });
    shell_manager.register_env_var("socket",
                                   [](const String& name, const Context& context)
                                   { return Server::instance().filename(); });
    shell_manager.register_env_var("client",
                                   [](const String& name, const Context& context)
                                   { return ClientManager::instance().get_client_name(context); });
    shell_manager.register_env_var("cursor_line",
                                   [](const String& name, const Context& context)
                                   { return int_to_str((int)context.editor().main_selection().last().line() + 1); });
    shell_manager.register_env_var("cursor_column",
                                   [](const String& name, const Context& context)
                                   { return int_to_str((int)context.editor().main_selection().last().column() + 1); });
    shell_manager.register_env_var("selection_desc",
                                   [](const String& name, const Context& context)
                                   { auto& sel = context.editor().main_selection();
                                     auto beg = sel.begin();
                                     return int_to_str((int)beg.line() + 1) + ':' + int_to_str((int)beg.column() + 1) + '+' + int_to_str((int)(sel.end() - beg)); });
}

void register_registers()
{
    RegisterManager& register_manager = RegisterManager::instance();

    register_manager.register_dynamic_register('%', [](const Context& context) { return std::vector<String>(1, context.buffer().display_name()); });
    register_manager.register_dynamic_register('.', [](const Context& context) { return context.editor().selections_content(); });
    for (size_t i = 0; i < 10; ++i)
    {
        register_manager.register_dynamic_register('0'+i,
            [i](const Context& context) {
                std::vector<String> result;
                for (auto& sel : context.editor().selections())
                    result.emplace_back(i < sel.captures().size() ? sel.captures()[i] : "");
                return result;
            });
    }
}

void create_local_client(const String& init_command)
{
    class LocalNCursesUI : public NCursesUI
    {
        ~LocalNCursesUI()
        {
            if (not ClientManager::instance().empty() and fork())
            {
                this->NCursesUI::~NCursesUI();
                puts("detached from terminal\n");
                exit(0);
            }
        }
    };

    UserInterface* ui = new LocalNCursesUI{};
    ClientManager::instance().create_client(
        std::unique_ptr<UserInterface>{ui}, init_command);
}

void signal_handler(int signal)
{
    endwin();
    const char* text = nullptr;
    switch (signal)
    {
        case SIGSEGV: text = "SIGSEGV"; break;
        case SIGFPE:  text = "SIGFPE";  break;
        case SIGQUIT: text = "SIGQUIT"; break;
        case SIGTERM: text = "SIGTERM"; break;
    }
    on_assert_failed(text);
    abort();
}

int main(int argc, char* argv[])
{
    try
    {
        std::locale::global(std::locale(""));

        signal(SIGSEGV, signal_handler);
        signal(SIGFPE,  signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTERM, signal_handler);

        std::vector<String> params;
        for (size_t i = 1; i < argc; ++i)
             params.push_back(argv[i]);
        ParametersParser parser(params, { { "c", true }, { "e", true }, { "n", false } });
        EventManager event_manager;

        String init_command;
        if (parser.has_option("e"))
            init_command = parser.option_value("e");

        if (parser.has_option("c"))
        {
            try
            {
                auto client = connect_to(parser.option_value("c"),
                                         std::unique_ptr<UserInterface>{new NCursesUI{}},
                                         init_command);
                while (true)
                    event_manager.handle_next_events();
            }
            catch (peer_disconnected&)
            {
                puts("disconnected");
            }
            return 0;
        }

        GlobalOptions       global_options;
        GlobalHooks         global_hooks;
        ShellManager        shell_manager;
        CommandManager      command_manager;
        BufferManager       buffer_manager;
        RegisterManager     register_manager;
        HighlighterRegistry highlighter_registry;
        FilterRegistry      filter_registry;
        ColorRegistry       color_registry;
        ClientManager       client_manager;

        run_unit_tests();

        register_env_vars();
        register_registers();
        register_commands();
        register_highlighters();
        register_filters();

        write_debug("*** This is the debug buffer, where debug info will be written ***");
        write_debug("pid: " + int_to_str(getpid()));
        write_debug("utf-8 test: é á ï");

        Server server;

        if (not parser.has_option("n")) try
        {
            Context initialisation_context;
            command_manager.execute("source " + runtime_directory() + "/kakrc",
                                    initialisation_context);
        }
        catch (Kakoune::runtime_error& error)
        {
             write_debug("error while parsing kakrc: "_str + error.what());
        }

        if (parser.positional_count() != 0)
        {
            // create buffers in reverse order so that the first given buffer
            // is the most recently created one.
            for (int i = parser.positional_count() - 1; i >= 0; --i)
            {
                const String& file = parser[i];
                if (not create_buffer_from_file(file))
                    new Buffer(file, Buffer::Flags::New | Buffer::Flags::File);
            }
        }
        else
            new Buffer("*scratch*", Buffer::Flags::None);

        create_local_client(init_command);

        while (not client_manager.empty())
            event_manager.handle_next_events();
    }
    catch (Kakoune::exception& error)
    {
        on_assert_failed(("uncaught exception:\n"_str + error.what()).c_str());
        return -1;
    }
    catch (std::exception& error)
    {
        on_assert_failed(("uncaught exception:\n"_str + error.what()).c_str());
        return -1;
    }
    catch (...)
    {
        on_assert_failed("uncaught exception");
        return -1;
    }
    return 0;
}
