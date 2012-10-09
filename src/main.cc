#include "window.hh"
#include "buffer.hh"
#include "shell_manager.hh"
#include "commands.hh"
#include "command_manager.hh"
#include "buffer_manager.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "assert.hh"
#include "debug.hh"
#include "highlighters.hh"
#include "highlighter_registry.hh"
#include "filters.hh"
#include "filter_registry.hh"
#include "hook_manager.hh"
#include "option_manager.hh"
#include "event_manager.hh"
#include "context.hh"
#include "ncurses.hh"
#include "string.hh"
#include "color_registry.hh"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <unordered_map>
#include <sys/types.h>
#include <sys/wait.h>

using namespace Kakoune;
using namespace std::placeholders;

namespace Kakoune
{

bool quit_requested = false;

template<InsertMode mode>
void do_insert(Context& context)
{
    context.client().insert(context, mode);
}

void do_repeat_insert(Context& context)
{
    context.client().repeat_last_insert(context);
}

template<SelectMode mode>
void do_go(Context& context)
{
    int count = context.numeric_param();
    if (count != 0)
    {
        BufferIterator target =
            context.editor().buffer().iterator_at_line_begin(count-1);

        context.editor().select(target);
        if (context.has_window())
            context.window().center_selection();
    }
    else
        context.client().on_next_key([](const Key& key, Context& context) {
            if (key.modifiers != Key::Modifiers::None)
                return;

            Editor& editor = context.editor();
            switch (key.key)
            {
            case 'g':
            case 't':
                editor.select(editor.buffer().begin());
                break;
            case 'l':
            case 'L':
                editor.select(select_to_eol, mode);
                break;
            case 'h':
            case 'H':
                editor.select(select_to_eol_reverse, mode);
                break;
            case 'b':
            {
                const Buffer& buf = editor.buffer();
                editor.select(buf.iterator_at_line_begin(buf.line_count() - 1));
                break;
            }
            }
        });
}

void do_replace_with_char(Context& context)
{
    context.client().on_next_key([](const Key& key, Context& context) {
        context.editor().insert(String() + key.key, InsertMode::Replace);
    });
}

void do_command(Context& context)
{
    context.client().prompt(
        ":", std::bind(&CommandManager::complete, &CommandManager::instance(), _1, _2, _3),
        [](const String& cmdline, Context& context) { CommandManager::instance().execute(cmdline, context); },
        context);
}

void do_pipe(Context& context)
{
    context.client().prompt("|", complete_nothing,
        [](const String& cmdline, Context& context)
        {
            Editor& editor = context.editor();
            std::vector<String> strings;
            for (auto& sel : const_cast<const Editor&>(context.editor()).selections())
                strings.push_back(ShellManager::instance().pipe(String(sel.begin(), sel.end()),
                                                                cmdline, context, {}, {}));
            editor.insert(strings, InsertMode::Replace);
        }, context);

}

template<SelectMode mode>
void do_search(Context& context)
{
    context.client().prompt("/", complete_nothing,
        [](const String& str, Context& context) {
            String ex = str;
            if (ex.empty())
                ex = RegisterManager::instance()['/'].values(context)[0];
            else
                RegisterManager::instance()['/'] = ex;

            context.editor().select(std::bind(select_next_match, _1, ex), mode);
        }, context);
}

template<SelectMode mode>
void do_search_next(Context& context)
{
    const String& ex = RegisterManager::instance()['/'].values(context)[0];
    if (not ex.empty())
        context.editor().select(std::bind(select_next_match, _1, ex), mode);
    else
        context.print_status("no search pattern");
}

void do_yank(Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
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
        return InsertMode::AppendAtLineEnd;
    if (mode == InsertMode::Insert)
        return InsertMode::InsertAtLineBegin;

    assert(false);
    return InsertMode::Insert;
}

template<InsertMode insert_mode>
void do_paste(Context& context)
{
    Editor& editor = context.editor();
    int count = context.numeric_param();
    auto strings = RegisterManager::instance()['"'].values(context);
    InsertMode mode = insert_mode;
    if (count == 0)
    {
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
    else if (count <= strings.size())
    {
        auto& str = strings[count-1];
        if (not str.empty() and str.back() == '\n')
            mode = adapt_for_linewise(mode);
        editor.insert(str, mode);
    }
}

void do_select_regex(Context& context)
{
    context.client().prompt("select: ", complete_nothing,
        [](const String& ex, Context& context)
        { context.editor().multi_select(std::bind(select_all_matches, _1, ex)); },
        context);
}

void do_split_regex(Context& context)
{
    context.client().prompt("select: ", complete_nothing,
        [](const String& ex, Context& context)
        { context.editor().multi_select(std::bind(split_selection, _1, ex)); },
        context);
}

void do_join(Context& context)
{
    Editor& editor = context.editor();
    editor.select(select_whole_lines);
    editor.select(select_to_eol, SelectMode::Extend);
    editor.multi_select(std::bind(select_all_matches, _1, "\n\\h*"));
    editor.insert(" ", InsertMode::Replace);
    editor.clear_selections();
    editor.move_selections(-1_char);
}

template<bool inner>
void do_select_object(Context& context)
{
    context.client().on_next_key(
    [](const Key& key, Context& context) {
        typedef std::function<SelectionAndCaptures (const Selection&)> Selector;
        static const std::unordered_map<Key, Selector> key_to_selector =
        {
            { { Key::Modifiers::None, '(' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '(', ')' }, inner) },
            { { Key::Modifiers::None, ')' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '(', ')' }, inner) },
            { { Key::Modifiers::None, 'b' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '(', ')' }, inner) },
            { { Key::Modifiers::None, '{' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '{', '}' }, inner) },
            { { Key::Modifiers::None, '}' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '{', '}' }, inner) },
            { { Key::Modifiers::None, 'B' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '{', '}' }, inner) },
            { { Key::Modifiers::None, '[' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '[', ']' }, inner) },
            { { Key::Modifiers::None, ']' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '[', ']' }, inner) },
            { { Key::Modifiers::None, '<' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '<', '>' }, inner) },
            { { Key::Modifiers::None, '>' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '<', '>' }, inner) },
            { { Key::Modifiers::None, 'w' }, std::bind(select_whole_word<false>, _1, inner) },
            { { Key::Modifiers::None, 'W' }, std::bind(select_whole_word<true>, _1, inner) },
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
    BufferCoord position = window.position();
    BufferIterator cursor_pos;

    if (key == Key::PageUp)
    {
        position.line -= (window.dimensions().line - 2);
        cursor_pos = buffer.iterator_at(position);
    }
    else if (key == Key::PageDown)
    {
        position.line += (window.dimensions().line - 2);
        cursor_pos = buffer.iterator_at(position + BufferCoord{window.dimensions().line - 1, 0});
    }

    window.select(cursor_pos);
    window.set_position(position);
}

template<typename T>
class Repeated
{
public:
    Repeated(T t) : m_func(t) {}

    void operator() (Context& context)
    {
        int count = context.numeric_param();
        do { m_func(context); } while(--count > 0);
    }
private:
    T m_func;
};

template<typename T>
Repeated<T> repeated(T func) { return Repeated<T>(func); }

namespace SelectFlags
{
    enum Type
    {
        None = 0,
        Reverse = 1,
        Inclusive = 2,
        Extend = 4
    };
}

template<int flags>
void select_to_next_char(Context& context)
{
    int param = context.numeric_param();
    context.client().on_next_key([param](const Key& key, Context& context) {
        context.editor().select(
            std::bind(flags & SelectFlags::Reverse ? select_to_reverse : select_to,
                      _1, key.key, param, flags & SelectFlags::Inclusive),
            flags & SelectFlags::Extend ? SelectMode::Extend : SelectMode::Replace);
   });
}

String runtime_directory()
{
    char buffer[2048];
#if defined(__linux__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048);
    assert(res != -1);
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
        throw runtime_error("unable do determine runtime directory");
    return String(buffer, ptr);
}

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

    { { Key::Modifiers::None, 'y' }, do_yank },
    { { Key::Modifiers::None, 'p' }, do_paste<InsertMode::Append> },
    { { Key::Modifiers::None, 'P' }, do_paste<InsertMode::Insert> },
    { { Key::Modifiers::Alt,  'p' }, do_paste<InsertMode::Replace> },

    { { Key::Modifiers::None, 's' }, do_select_regex },

    { { Key::Modifiers::None, '.' }, do_repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Context& context) { context.editor().clear_selections(); context.editor().select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, do_command },
    { { Key::Modifiers::None, '|' }, do_pipe },
    { { Key::Modifiers::None, ' ' }, [](Context& context) { int count = context.numeric_param();
                                                            if (count == 0) context.editor().clear_selections();
                                                            else context.editor().keep_selection(count-1); } },
    { { Key::Modifiers::Alt,  ' ' }, [](Context& context) { int count = context.numeric_param();
                                                            if (count == 0) context.editor().clear_selections();
                                                            else context.editor().remove_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, repeated([](Context& context) { context.editor().select(select_to_next_word<false>); }) },
    { { Key::Modifiers::None, 'e' }, repeated([](Context& context) { context.editor().select(select_to_next_word_end<false>); }) },
    { { Key::Modifiers::None, 'b' }, repeated([](Context& context) { context.editor().select(select_to_previous_word<false>); }) },
    { { Key::Modifiers::None, 'W' }, repeated([](Context& context) { context.editor().select(select_to_next_word<false>, SelectMode::Extend); }) },
    { { Key::Modifiers::None, 'E' }, repeated([](Context& context) { context.editor().select(select_to_next_word_end<false>, SelectMode::Extend); }) },
    { { Key::Modifiers::None, 'B' }, repeated([](Context& context) { context.editor().select(select_to_previous_word<false>, SelectMode::Extend); }) },
    { { Key::Modifiers::None, 'x' }, repeated([](Context& context) { context.editor().select(select_line, SelectMode::Replace); }) },
    { { Key::Modifiers::None, 'X' }, repeated([](Context& context) { context.editor().select(select_line, SelectMode::Extend); }) },
    { { Key::Modifiers::None, 'm' }, [](Context& context) { context.editor().select(select_matching); } },
    { { Key::Modifiers::None, 'M' }, [](Context& context) { context.editor().select(select_matching, SelectMode::Extend); } },

    { { Key::Modifiers::None, '/' }, do_search<SelectMode::Replace> },
    { { Key::Modifiers::None, '?' }, do_search<SelectMode::Extend> },
    { { Key::Modifiers::None, 'n' }, do_search_next<SelectMode::Replace> },
    { { Key::Modifiers::None, 'N' }, do_search_next<SelectMode::Extend> },

    { { Key::Modifiers::None, 'u' }, repeated([](Context& context) { if (not context.editor().undo()) { context.print_status("nothing left to undo"); } }) },
    { { Key::Modifiers::None, 'U' }, repeated([](Context& context) { if (not context.editor().redo()) { context.print_status("nothing left to redo"); } }) },

    { { Key::Modifiers::Alt,  'i' }, do_select_object<true> },
    { { Key::Modifiers::Alt,  'a' }, do_select_object<false> },

    { { Key::Modifiers::Alt, 'w' }, repeated([](Context& context) { context.editor().select(select_to_next_word<true>); }) },
    { { Key::Modifiers::Alt, 'e' }, repeated([](Context& context) { context.editor().select(select_to_next_word_end<true>); }) },
    { { Key::Modifiers::Alt, 'b' }, repeated([](Context& context) { context.editor().select(select_to_previous_word<true>); }) },
    { { Key::Modifiers::Alt, 'W' }, repeated([](Context& context) { context.editor().select(select_to_next_word<true>, SelectMode::Extend); }) },
    { { Key::Modifiers::Alt, 'E' }, repeated([](Context& context) { context.editor().select(select_to_next_word_end<true>, SelectMode::Extend); }) },
    { { Key::Modifiers::Alt, 'B' }, repeated([](Context& context) { context.editor().select(select_to_previous_word<true>, SelectMode::Extend); }) },

    { { Key::Modifiers::Alt, 'l' }, repeated([](Context& context) { context.editor().select(select_to_eol, SelectMode::Replace); }) },
    { { Key::Modifiers::Alt, 'L' }, repeated([](Context& context) { context.editor().select(select_to_eol, SelectMode::Extend); }) },
    { { Key::Modifiers::Alt, 'h' }, repeated([](Context& context) { context.editor().select(select_to_eol_reverse, SelectMode::Replace); }) },
    { { Key::Modifiers::Alt, 'H' }, repeated([](Context& context) { context.editor().select(select_to_eol_reverse, SelectMode::Extend); }) },

    { { Key::Modifiers::Alt, 's' }, do_split_regex },

    { { Key::Modifiers::Alt, 'j' }, do_join },

    { { Key::Modifiers::Alt, 'x' }, [](Context& context) { context.editor().select(select_whole_lines); } },

    { { Key::Modifiers::Alt, 'c' }, [](Context& context) { if (context.has_window()) context.window().center_selection(); } },

    { { Key::Modifiers::None, Key::PageUp }, do_scroll<Key::PageUp> },
    { { Key::Modifiers::None, Key::PageDown }, do_scroll<Key::PageDown> },
};

}

void run_unit_tests();

int main(int argc, char* argv[])
{
    EventManager        event_manager;
    GlobalOptionManager option_manager;
    GlobalHookManager   hook_manager;
    ShellManager        shell_manager;
    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    FilterRegistry      filter_registry;
    ColorRegistry       color_registry;

    run_unit_tests();

    shell_manager.register_env_var("bufname",
                                   [](const String& name, const Context& context)
                                   { return context.buffer().name(); });
    shell_manager.register_env_var("selection",
                                   [](const String& name, const Context& context)
                                   { return context.editor().selections_content().back(); });
    shell_manager.register_env_var("runtime",
                                   [](const String& name, const Context& context)
                                   { return runtime_directory(); });
    shell_manager.register_env_var("opt_.+",
                                   [](const String& name, const Context& context)
                                   { return context.option_manager()[name.substr(4)].as_string(); });
    shell_manager.register_env_var("reg_.+",
                                   [](const String& name, const Context& context)
                                   { return RegisterManager::instance()[name[4]].values(context)[0]; });

    register_manager.register_dynamic_register('%', [](const Context& context) { return std::vector<String>(1, context.buffer().name()); });
    register_manager.register_dynamic_register('.', [](const Context& context) { return context.editor().selections_content(); });
    for (size_t i = 0; i < 10; ++i)
    {
         register_manager.register_dynamic_register('0'+i,
              [i](const Context& context) {
                  std::vector<String> result;
                  for (auto& sel_and_cap : context.editor().selections())
                      result.emplace_back(i < sel_and_cap.captures.size() ? sel_and_cap.captures[i] : "");
                  return result;
              });
    }

    register_commands();
    register_highlighters();
    register_filters();

    try
    {
        Client client;
        NCursesUI ui;

        Context context(client);
        context.change_ui(ui);

        try
        {
            Context initialisation_context;
            command_manager.execute("source " + runtime_directory() + "/kakrc",
                                    initialisation_context);
        }
        catch (Kakoune::runtime_error& error)
        {
            context.print_status(error.description());
        }


        write_debug("*** This is the debug buffer, where debug info will be written ***\n");
        write_debug("utf-8 test: é á ï");

        if (argc > 1)
        {
            String cmd = "edit ";
            for (int i = 1; i < argc; ++i)
                cmd += String(" ") + argv[i];
            command_manager.execute(cmd, context);
        }
        else
        {
            auto buffer = new Buffer("*scratch*", Buffer::Type::Scratch);
            context.change_editor(*buffer->get_or_create_window());
        }

        event_manager.watch(0, [&](int) { client.handle_next_input(context); });

        context.draw_ifn();
        while(not quit_requested)
        {
            try
            {
                event_manager.handle_next_events();
            }
            catch (Kakoune::runtime_error& error)
            {
                context.print_status(error.description());
            }
        }
    }
    catch (Kakoune::exception& error)
    {
        puts("uncaught exception:\n");
        puts(error.description().c_str());
        return -1;
    }
    return 0;
}
