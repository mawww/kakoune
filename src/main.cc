#include "window.hh"
#include "buffer.hh"
#include "file.hh"
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
#include "context.hh"
#include "ncurses.hh"
#include "regex.hh"

#include <unordered_map>
#include <sys/types.h>
#include <sys/wait.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

using namespace Kakoune;
using namespace std::placeholders;

void draw_editor_ifn(Editor& editor)
{
    Window* window = dynamic_cast<Window*>(&editor);
    if (window)
        NCurses::draw_window(*window);
}

PromptFunc prompt_func;
String prompt(const String& text, Completer completer = complete_nothing)
{
    return prompt_func(text, completer);
}

GetKeyFunc get_key_func;
Key get_key()
{
    return get_key_func();
}

struct InsertSequence
{
    IncrementalInserter::Mode mode;
    std::vector<Key>          keys;

    InsertSequence() : mode(IncrementalInserter::Mode::Insert) {}
};

InsertSequence last_insert_sequence;

bool insert_char(IncrementalInserter& inserter, const Key& key)
{
    switch (key.modifiers)
    {
    case Key::Modifiers::None:
        switch (key.key)
        {
        case 27:
            return false;
        default:
            inserter.insert(String() + key.key);
        }
        break;
    case Key::Modifiers::Control:
        switch (key.key)
        {
        case 'r':
        {
            Key next_key = get_key();
            last_insert_sequence.keys.push_back(next_key);
            if (next_key.modifiers == Key::Modifiers::None)
            {
                switch (next_key.key)
                {
                case '%':
                    inserter.insert(inserter.buffer().name());
                    break;
                default:
                    inserter.insert(RegisterManager::instance()[next_key.key]);
                }
            }
            break;
        }
        case 'm':
            inserter.insert(String() + '\n');
            break;
        case 'i':
            inserter.insert(String() + '\t');
            break;
        case 'd':
            inserter.move_cursors({0, -1});
            break;
        case 'e':
            inserter.move_cursors({0,  1});
            break;
        case 'g':
            inserter.erase();
            break;
        }
        break;
    }
    return true;
}

void do_insert(Editor& editor, IncrementalInserter::Mode mode)
{
    last_insert_sequence.mode = mode;
    last_insert_sequence.keys.clear();
    IncrementalInserter inserter(editor, mode);
    draw_editor_ifn(editor);
    while(true)
    {
        Key key = get_key();

        if (not insert_char(inserter, key))
            break;

        last_insert_sequence.keys.push_back(key);
        draw_editor_ifn(editor);
    }
}

void do_repeat_insert(Editor& editor, int count)
{
    IncrementalInserter inserter(editor, last_insert_sequence.mode);
    for (const Key& key : last_insert_sequence.keys)
    {
        insert_char(inserter, key);
    }
    draw_editor_ifn(editor);
}


template<bool append>
void do_go(Editor& editor, int count)
{
    if (count != 0)
    {
        BufferIterator target =
            editor.buffer().iterator_at(BufferCoord(count-1, 0));

        editor.select(target);
    }
    else
    {
        Key key = get_key();
        if (key.modifiers != Key::Modifiers::None)
            return;

        switch (key.key)
        {
        case 'g':
        case 't':
        {
            BufferIterator target =
                editor.buffer().iterator_at(BufferCoord(0,0));
            editor.select(target);
            break;
        }
        case 'l':
        case 'L':
            editor.select(select_to_eol, append);
            break;
        case 'h':
        case 'H':
            editor.select(select_to_eol_reverse, append);
            break;
        case 'b':
        {
            BufferIterator target = editor.buffer().iterator_at(
                BufferCoord(editor.buffer().line_count() - 1, 0));
            editor.select(target);
            break;
        }
        }
    }
}

Context main_context;

Buffer* open_or_create(const String& filename)
{
    Buffer* buffer = NULL;
    try
    {
        buffer = create_buffer_from_file(filename);
    }
    catch (file_not_found& what)
    {
        NCurses::print_status("new file " + filename);
        buffer = new Buffer(filename, Buffer::Type::NewFile);
    }
    return buffer;
}

template<bool force_reload>
void edit(const CommandParameters& params, const Context& context)
{
    if (params.size() == 0 or params.size() > 3)
        throw wrong_argument_count();

    String filename = params[0];

    Buffer* buffer = nullptr;
    if (not force_reload)
        buffer = BufferManager::instance().get_buffer(filename);
    if (not buffer)
        buffer = open_or_create(filename);

    Window& window = *buffer->get_or_create_window();

    if (params.size() > 1)
    {
        int line = std::max(0, atoi(params[1].c_str()) - 1);
        int column = params.size() > 2 ?
                         std::max(0, atoi(params[2].c_str()) - 1) : 0;

        window.select(window.buffer().iterator_at({line, column}));
    }

    main_context = Context(window);
}

void write_buffer(const CommandParameters& params, const Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    Buffer& buffer = context.window().buffer();
    String filename = params.empty() ? buffer.name()
                                          : parse_filename(params[0]);

    write_buffer_to_file(buffer, filename);
    buffer.notify_saved();
}

bool quit_requested = false;

template<bool force>
void quit(const CommandParameters& params, const Context& context)
{
    if (params.size() != 0)
        throw wrong_argument_count();

    if (not force)
    {
        std::vector<String> names;
        for (auto& buffer : BufferManager::instance())
        {
            if (buffer.type() != Buffer::Type::Scratch and buffer.is_modified())
                names.push_back(buffer.name());
        }
        if (not names.empty())
        {
            String message = "modified buffers remaining: [";
            for (auto it = names.begin(); it != names.end(); ++it)
            {
                if (it != names.begin())
                    message += ", ";
                message += *it;
            }
            message += "]";
            NCurses::print_status(message);
            return;
        }
    }
    quit_requested = true;
}

template<bool force>
void write_and_quit(const CommandParameters& params, const Context& context)
{
    write_buffer(params, context);
    quit<force>(CommandParameters(), context);
}

void show_buffer(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    Buffer* buffer = BufferManager::instance().get_buffer(params[0]);
    if (not buffer)
        NCurses::print_status("buffer " + params[0] + " does not exists");
    else
        main_context = Context(*buffer->get_or_create_window());
}

void add_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 1)
        throw wrong_argument_count();

    try
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();
        HighlighterParameters highlighter_params(params.begin()+1, params.end());
        registry.add_highlighter_to_window(context.window(), params[0],
                                           highlighter_params);
    }
    catch (runtime_error& err)
    {
        NCurses::print_status("error: " + err.description());
    }
}

void add_group_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 2)
        throw wrong_argument_count();

    try
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        HighlighterGroup& group = context.window().highlighters().get_group(params[0]);
        HighlighterParameters highlighter_params(params.begin()+2, params.end());
        registry.add_highlighter_to_group(context.window(), group,
                                          params[1], highlighter_params);
    }
    catch (runtime_error& err)
    {
        NCurses::print_status("error: " + err.description());
    }
}

void rm_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    context.window().highlighters().remove(params[0]);
}

void rm_group_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() != 2)
        throw wrong_argument_count();

    try
    {
        HighlighterGroup& group = context.window().highlighters().get_group(params[0]);
        group.remove(params[1]);
    }
    catch (runtime_error& err)
    {
        NCurses::print_status("error: " + err.description());
    }
}
void add_filter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 1)
        throw wrong_argument_count();

    try
    {
        FilterRegistry& registry = FilterRegistry::instance();
        FilterParameters filter_params(params.begin()+1, params.end());
        registry.add_filter_to_window(context.window(), params[0],
                                      filter_params);
    }
    catch (runtime_error& err)
    {
        NCurses::print_status("error: " + err.description());
    }
}

void rm_filter(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    context.window().remove_filter(params[0]);
}

void add_hook(const CommandParameters& params, const Context& context)
{
    if (params.size() < 4)
        throw wrong_argument_count();

    String regex = params[2];
    std::vector<String> hook_params(params.begin()+3, params.end());

    auto hook_func = [=](const String& param, const Context& context) {
        if (boost::regex_match(param.begin(), param.end(),
                               Regex(regex.begin(), regex.end())))
            CommandManager::instance().execute(hook_params, context);
    };

    if (params[0] == "global")
        GlobalHookManager::instance().add_hook(params[1], hook_func);
    else if (params[0] == "window")
        context.window().hook_manager().add_hook(params[1], hook_func);
    else
        NCurses::print_status("error: no such hook container " + params[0]);
}

void define_command(const CommandParameters& params, const Context& context)
{
    if (params.size() < 2)
        throw wrong_argument_count();

    if (params[0] == "-env-params")
    {
        std::vector<String> cmd_params(params.begin() + 2, params.end());
        CommandManager::instance().register_command(params[1],
             [=](const CommandParameters& params, const Context& context) {
                char param_name[] = "kak_param0";
                for (size_t i = 0; i < 10; ++i)
                {
                    param_name[sizeof(param_name) - 2] = '0' + i;
                    if (params.size() > i)
                        setenv(param_name, params[i].c_str(), 1);
                    else
                        unsetenv(param_name);
                }
                CommandManager::instance().execute(cmd_params, context);
            });
    }
    else if (params[0] == "-append-params")
    {
        std::vector<String> cmd_params(params.begin() + 2, params.end());
        CommandManager::instance().register_command(params[1],
             [=](const CommandParameters& params, const Context& context) {
                std::vector<String> merged_params = cmd_params;
                for (auto& param : params)
                    merged_params.push_back(param);
                CommandManager::instance().execute(merged_params, context);
            });
    }
    else
    {
        std::vector<String> cmd_params(params.begin() + 1, params.end());
        CommandManager::instance().register_command(params[0],
             [=](const CommandParameters& params, const Context& context) {
                 if (not params.empty())
                     throw wrong_argument_count();
                CommandManager::instance().execute(cmd_params, context);
            });
    }
}

void echo_message(const CommandParameters& params, const Context& context)
{
    String message;
    for (auto& param : params)
        message += param + " ";
    NCurses::print_status(message);
}

void exec_commands_in_file(const CommandParameters& params,
                           const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    String file_content = read_file(parse_filename(params[0]));
    CommandManager& cmd_manager = CommandManager::instance();

    size_t pos = 0;
    size_t length = file_content.length();
    bool   cat_with_previous = false;
    String command_line;
    while (true)
    {
         if (not cat_with_previous)
             command_line.clear();

         size_t end_pos = pos;

         while (file_content[end_pos] != '\n' and end_pos != length)
         {
             if (file_content[end_pos] == '"' or file_content[end_pos] == '\'' or
                 file_content[end_pos] == '`')
             {
                 char delimiter = file_content[end_pos];
                 ++end_pos;
                 while ((file_content[end_pos] != delimiter or
                         file_content[end_pos-1] == '\\') and end_pos != length)
                     ++end_pos;

                 if (end_pos == length)
                 {
                     NCurses::print_status(String("unterminated '") + delimiter + "' string");
                     return;
                 }

                 ++end_pos;
             }

             if (end_pos != length)
                 ++end_pos;
         }
         if (end_pos != pos and end_pos != length and
             file_content[end_pos - 1] == '\\')
         {
 	     command_line += file_content.substr(pos, end_pos - pos - 1);
             cat_with_previous = true;
         }
         else
         {
 	     command_line += file_content.substr(pos, end_pos - pos);
 	     cmd_manager.execute(command_line, context);
             cat_with_previous = false;
 	 }
         if (end_pos == length)
         {
             if (cat_with_previous)
                 NCurses::print_status("while executing commands in \"" + params[0] +
                              "\": last command not complete");
             break;
         }
         pos = end_pos + 1;
    }
}

void exec_commands_in_runtime_file(const CommandParameters& params,
                                   const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    const String& filename = params[0];
    char buffer[2048];
#if defined(__linux__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048 - filename.length());
    assert(res != -1);
    buffer[res] = '\0';
#elif defined(__APPLE__)
    uint32_t bufsize = 2048 - filename.length();
    _NSGetExecutablePath(buffer, &bufsize);
#else
# error "finding executable path is not implemented on this platform"
#endif
    char* ptr = strrchr(buffer, '/');
    if (ptr)
    {
        strcpy(ptr+1, filename.c_str());
        exec_commands_in_file(CommandParameters(buffer), main_context);
    }
}

void set_option(OptionManager& option_manager, const CommandParameters& params)
{
    if (params.size() != 2)
        throw wrong_argument_count();

    option_manager[params[0]] = params[1];
}

void do_command()
{
    try
    {
        auto cmdline = prompt(":", std::bind(&CommandManager::complete,
                                             &CommandManager::instance(),
                                             _1, _2));

        CommandManager::instance().execute(cmdline, main_context);
    }
    catch (prompt_aborted&) {}
}

void do_pipe(Editor& editor, int count)
{
    try
    {
        auto cmdline = prompt("|", complete_nothing);

        editor.buffer().begin_undo_group();
        for (auto& sel : const_cast<const Editor&>(editor).selections())
        {
            int write_pipe[2];
            int read_pipe[2];

            pipe(write_pipe);
            pipe(read_pipe);

            if (pid_t pid = fork())
            {
                close(write_pipe[0]);
                close(read_pipe[1]);

                String content = editor.buffer().string(sel.begin(), sel.end());
                memoryview<char> data = content.data();
                write(write_pipe[1], data.pointer(), data.size());
                close(write_pipe[1]);

                String new_content;
                char buffer[1024];
                while (size_t size = read(read_pipe[0], buffer, 1024))
                {
                    new_content += String(buffer, buffer+size);
                }
                close(read_pipe[0]);
                waitpid(pid, NULL, 0);

                editor.buffer().modify(Modification::make_erase(sel.begin(), sel.end()));
                editor.buffer().modify(Modification::make_insert(sel.begin(), new_content));
            }
            else
            {
                close(write_pipe[1]);
                close(read_pipe[0]);

                dup2(read_pipe[1], 1);
                dup2(write_pipe[0], 0);

                execlp("sh", "sh", "-c", cmdline.c_str(), NULL);
            }
        }
        editor.buffer().end_undo_group();
    }
    catch (prompt_aborted&) {}
}

template<bool append>
void do_search(Editor& editor)
{
    try
    {
        String ex = prompt("/");
        if (ex.empty())
            ex = RegisterManager::instance()['/'].get();
        else
            RegisterManager::instance()['/'] = ex;

        editor.select(std::bind(select_next_match, _1, ex), append);
    }
    catch (prompt_aborted&) {}
}

template<bool append>
void do_search_next(Editor& editor)
{
    const String& ex = RegisterManager::instance()['/'].get();
    if (not ex.empty())
        editor.select(std::bind(select_next_match, _1, ex), append);
    else
        NCurses::print_status("no search pattern");
}

void do_yank(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
}

void do_erase(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
    editor.erase();
}

void do_change(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
    do_insert(editor, IncrementalInserter::Mode::Change);
}

template<bool append>
void do_paste(Editor& editor, int count)
{
    Register& reg = RegisterManager::instance()['"'];
    if (count == 0)
    {
        if (append)
            editor.append(reg);
        else
            editor.insert(reg);
    }
    else
    {
        if (append)
            editor.append(reg.get(count-1));
        else
            editor.insert(reg.get(count-1));
    }
}

void do_select_regex(Editor& editor, int count)
{
    try
    {
        String ex = prompt("select: ");
        editor.multi_select(std::bind(select_all_matches, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_split_regex(Editor& editor, int count)
{
    try
    {
        String ex = prompt("split: ");
        editor.multi_select(std::bind(split_selection, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_join(Editor& editor, int count)
{
    editor.select(select_whole_lines);
    editor.select(select_to_eol, true);
    editor.multi_select(std::bind(select_all_matches, _1, "\n\\h*"));
    editor.replace(" ");
    editor.clear_selections();
    editor.move_selections({0, -1});
}

template<bool inner>
void do_select_object(Editor& editor, int count)
{
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

    Key key = get_key();
    auto it = key_to_selector.find(key);
    if (it != key_to_selector.end())
        editor.select(it->second);
}

std::unordered_map<Key, std::function<void (Editor& editor, int count)>> keymap =
{
    { { Key::Modifiers::None, 'h' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0, -std::max(count,1))); } },
    { { Key::Modifiers::None, 'j' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord( std::max(count,1), 0)); } },
    { { Key::Modifiers::None, 'k' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(-std::max(count,1), 0)); } },
    { { Key::Modifiers::None, 'l' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0,  std::max(count,1))); } },

    { { Key::Modifiers::None, 'H' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0, -std::max(count,1)), true); } },
    { { Key::Modifiers::None, 'J' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord( std::max(count,1), 0), true); } },
    { { Key::Modifiers::None, 'K' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(-std::max(count,1), 0), true); } },
    { { Key::Modifiers::None, 'L' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0,  std::max(count,1)), true); } },

    { { Key::Modifiers::None, 't' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, false)); } },
    { { Key::Modifiers::None, 'f' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, true)); } },
    { { Key::Modifiers::None, 'T' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, false), true); } },
    { { Key::Modifiers::None, 'F' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, true), true); } },

    { { Key::Modifiers::None, 'd' }, do_erase },
    { { Key::Modifiers::None, 'c' }, do_change },
    { { Key::Modifiers::None, 'i' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::Insert); } },
    { { Key::Modifiers::None, 'I' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::InsertAtLineBegin); } },
    { { Key::Modifiers::None, 'a' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::Append); } },
    { { Key::Modifiers::None, 'A' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::AppendAtLineEnd); } },
    { { Key::Modifiers::None, 'o' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::OpenLineBelow); } },
    { { Key::Modifiers::None, 'O' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::OpenLineAbove); } },

    { { Key::Modifiers::None, 'g' }, do_go<false> },
    { { Key::Modifiers::None, 'G' }, do_go<true> },

    { { Key::Modifiers::None, 'y' }, do_yank },
    { { Key::Modifiers::None, 'p' }, do_paste<true> },
    { { Key::Modifiers::None, 'P' }, do_paste<false> },

    { { Key::Modifiers::None, 's' }, do_select_regex },


    { { Key::Modifiers::None, '.' }, do_repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Editor& editor, int count) { editor.clear_selections(); editor.select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, [](Editor& editor, int count) { do_command(); } },
    { { Key::Modifiers::None, '|' }, do_pipe },
    { { Key::Modifiers::None, ' ' }, [](Editor& editor, int count) { if (count == 0) editor.clear_selections();
                                                                     else editor.keep_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'e' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'b' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'W' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'E' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'B' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'x' }, [](Editor& editor, int count) { do { editor.select(select_line, false); } while(--count > 0); } },
    { { Key::Modifiers::None, 'X' }, [](Editor& editor, int count) { do { editor.select(select_line, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'm' }, [](Editor& editor, int count) { editor.select(select_matching); } },
    { { Key::Modifiers::None, 'M' }, [](Editor& editor, int count) { editor.select(select_matching, true); } },

    { { Key::Modifiers::None, '/' }, [](Editor& editor, int count) { do_search<false>(editor); } },
    { { Key::Modifiers::None, '?' }, [](Editor& editor, int count) { do_search<true>(editor); } },
    { { Key::Modifiers::None, 'n' }, [](Editor& editor, int count) { do_search_next<false>(editor); } },
    { { Key::Modifiers::None, 'N' }, [](Editor& editor, int count) { do_search_next<true>(editor); } },

    { { Key::Modifiers::None, 'u' }, [](Editor& editor, int count) { do { if (not editor.undo()) { NCurses::print_status("nothing left to undo"); break; } } while(--count > 0); } },
    { { Key::Modifiers::None, 'U' }, [](Editor& editor, int count) { do { if (not editor.redo()) { NCurses::print_status("nothing left to redo"); break; } } while(--count > 0); } },

    { { Key::Modifiers::Alt,  'i' }, do_select_object<true> },
    { { Key::Modifiers::Alt,  'a' }, do_select_object<false> },

    { { Key::Modifiers::Alt, 't' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, false)); } },
    { { Key::Modifiers::Alt, 'f' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, true)); } },
    { { Key::Modifiers::Alt, 'T' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, false), true); } },
    { { Key::Modifiers::Alt, 'F' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, true), true); } },

    { { Key::Modifiers::Alt, 'w' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'e' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'b' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'W' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<true>, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'E' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<true>, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'B' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<true>, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 'l' }, [](Editor& editor, int count) { do { editor.select(select_to_eol, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'L' }, [](Editor& editor, int count) { do { editor.select(select_to_eol, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'h' }, [](Editor& editor, int count) { do { editor.select(select_to_eol_reverse, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'H' }, [](Editor& editor, int count) { do { editor.select(select_to_eol_reverse, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 's' }, do_split_regex },

    { { Key::Modifiers::Alt, 'j' }, do_join },

    { { Key::Modifiers::Alt, 'x' }, [](Editor& editor, int count) { editor.select(select_whole_lines); } },
};

class RegisterRestorer
{
public:
    RegisterRestorer(char name)
       : m_name(name),
         m_save(RegisterManager::instance()[name].content())
    {}

    ~RegisterRestorer()
    { RegisterManager::instance()[m_name] = m_save; }

private:
    std::vector<String> m_save;
    char                m_name;
};

void exec_keys(const KeyList& keys,
               const Context& context)
{
    auto prompt_save = prompt_func;
    auto get_key_save = get_key_func;

    auto restore_funcs = on_scope_end([&]() {
        prompt_func = prompt_save;
        get_key_func = get_key_save;
    });

    size_t pos = 0;

    prompt_func = [&](const String&, Completer) {
        size_t begin = pos;
        while (pos < keys.size() and keys[pos].key != '\n')
            ++pos;

        String result;
        for (size_t i = begin; i < pos; ++i)
            result += String() + keys[i].key;
        ++pos;

        return result;
    };

    get_key_func = [&]() {
        if (pos >= keys.size())
            throw runtime_error("no more characters");
        return keys[pos++];
    };

    RegisterRestorer quote('"');
    RegisterRestorer slash('/');

    Editor batch_editor(context.buffer());
    Editor& editor = context.has_window() ? static_cast<Editor&>(context.window())
                                          : static_cast<Editor&>(batch_editor);

    scoped_edition edition(editor);

    int count = 0;
    while(pos < keys.size())
    {
        const Key& key = keys[pos++];

        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            count = count * 10 + key.key - '0';
        else
        {
            auto it = keymap.find(key);
            if (it != keymap.end())
                it->second(editor, count);
            count = 0;
        }
    }
}

void exec_string(const CommandParameters& params,
                 const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    KeyList keys = parse_keys(params[0]);

    exec_keys(keys, context);
}

void run_unit_tests();

int main(int argc, char* argv[])
{
    NCurses::init(prompt_func, get_key_func);

    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    FilterRegistry      filter_registry;
    GlobalHookManager   hook_manager;
    GlobalOptionManager option_manager;

    run_unit_tests();

    command_manager.register_commands({ "e", "edit" }, edit<false>,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_commands({ "e!", "edit!" }, edit<true>,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_commands({ "q", "quit" }, quit<false>);
    command_manager.register_commands({ "q!", "quit!" }, quit<true>);
    command_manager.register_commands({ "w", "write" }, write_buffer,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_command("wq", write_and_quit<false>);
    command_manager.register_command("wq!", write_and_quit<true>);
    command_manager.register_commands({ "b", "buffer" }, show_buffer,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         std::bind(&BufferManager::complete_buffername, &buffer_manager, _1, _2)
                                      }));
    command_manager.register_commands({ "ah", "addhl" }, add_highlighter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         std::bind(&HighlighterRegistry::complete_highlighter, &highlighter_registry, _1, _2)
                                     }));
    command_manager.register_commands({ "agh", "addgrouphl" }, add_group_highlighter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const String& prefix, size_t cursor_pos)
                                         { return main_context.window().highlighters().complete_group_id(prefix, cursor_pos); },
                                         std::bind(&HighlighterRegistry::complete_highlighter, &highlighter_registry, _1, _2)
                                     }));
    command_manager.register_commands({ "rh", "rmhl" }, rm_highlighter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const String& prefix, size_t cursor_pos)
                                         { return main_context.window().highlighters().complete_group_id(prefix, cursor_pos); }
                                     }));
    command_manager.register_commands({ "rgh", "rmgrouphl" }, rm_group_highlighter,
                                     CommandManager::None,
                                     [&](const CommandParameters& params, size_t token_to_complete, size_t pos_in_token)
                                     {
                                         Window& w = main_context.window();
                                         const String& arg = token_to_complete < params.size() ?
                                                                  params[token_to_complete] : String();
                                         if (token_to_complete == 0)
                                             return w.highlighters().complete_group_id(arg, pos_in_token);
                                         else if (token_to_complete == 1)
                                             return w.highlighters().get_group(params[0]).complete_id(arg, pos_in_token);
                                     });
    command_manager.register_commands({ "af", "addfilter" }, add_filter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         std::bind(&FilterRegistry::complete_filter, &filter_registry, _1, _2)
                                     }));
    command_manager.register_commands({ "rf", "rmfilter" }, rm_filter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const String& prefix, size_t cursor_pos)
                                         { return main_context.window().complete_filterid(prefix, cursor_pos); }
                                     }));
    command_manager.register_command("hook", add_hook, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);

    command_manager.register_command("source", exec_commands_in_file,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_command("runtime", exec_commands_in_runtime_file);

    command_manager.register_command("exec", exec_string);

    command_manager.register_command("def",   define_command, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);
    command_manager.register_command("echo", echo_message);

    command_manager.register_commands({ "setg", "setglobal" },
                                     [&](const CommandParameters& params, const Context&) { set_option(option_manager, params); },
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const String& prefix, size_t cursor_pos)
                                         { return option_manager.complete_option_name(prefix, cursor_pos); }
                                     }));
    command_manager.register_commands({ "setb", "setbuffer" },
                                     [&](const CommandParameters& params, const Context& context)
                                     { set_option(context.buffer().option_manager(), params); },
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const String& prefix, size_t cursor_pos)
                                         { return main_context.buffer().option_manager().complete_option_name(prefix, cursor_pos); }
                                     }));
    command_manager.register_commands({ "setw", "setwindow" },
                                     [&](const CommandParameters& params, const Context& context)
                                     { set_option(context.window().option_manager(), params); },
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const String& prefix, size_t cursor_pos)
                                         { return main_context.window().option_manager().complete_option_name(prefix, cursor_pos); }
                                     }));

    register_highlighters();
    register_filters();

    try
    {
        exec_commands_in_runtime_file({ "kakrc" }, main_context);
    }
     catch (Kakoune::runtime_error& error)
    {
        NCurses::print_status(error.description());
    }

    try
    {
        write_debug("*** This is the debug buffer, where debug info will be written ***\n");
        write_debug("utf-8 test: é á ï");

        auto buffer = (argc > 1) ? open_or_create(argv[1]) : new Buffer("*scratch*", Buffer::Type::Scratch);
        main_context = Context(*buffer->get_or_create_window());

        NCurses::draw_window(main_context.window());
        int count = 0;
        while(not quit_requested)
        {
            try
            {
                Key key = get_key();
                if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
                    count = count * 10 + key.key - '0';
                else
                {
                    auto it = keymap.find(key);
                    if (it != keymap.end())
                    {
                        it->second(main_context.window(), count);
                        NCurses::draw_window(main_context.window());
                    }
                    count = 0;
                }
            }
            catch (Kakoune::runtime_error& error)
            {
                NCurses::print_status(error.description());
            }
        }
        NCurses::deinit();
    }
    catch (Kakoune::exception& error)
    {
        NCurses::deinit();
        puts("uncaught exception:\n");
        puts(error.description().c_str());
        return -1;
    }
    catch (...)
    {
        NCurses::deinit();
        throw;
    }
    return 0;
}
