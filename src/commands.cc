#include "commands.hh"

#include "command_manager.hh"
#include "buffer_manager.hh"
#include "option_manager.hh"
#include "context.hh"
#include "buffer.hh"
#include "window.hh"
#include "file.hh"
#include "ncurses.hh"
#include "regex.hh"
#include "highlighter_registry.hh"
#include "filter_registry.hh"
#include "register_manager.hh"
#include "completion.hh"


namespace Kakoune
{

using namespace std::placeholders;

// berk
extern Context main_context;
extern bool    quit_requested;

extern std::unordered_map<Key, std::function<void (Editor& editor, int count)>> keymap;
extern PromptFunc prompt_func;
extern GetKeyFunc get_key_func;

namespace
{

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

        if (params[0] == "-group")
        {
            if (params.size() < 3)
                throw wrong_argument_count();
            HighlighterParameters highlighter_params(params.begin()+3, params.end());
            HighlighterGroup& group = context.window().highlighters().get_group(params[1]);
            registry.add_highlighter_to_group(context.window(), group,
                                              params[2], highlighter_params);
        }
        else
        {
            HighlighterParameters highlighter_params(params.begin()+1, params.end());
            registry.add_highlighter_to_window(context.window(),
                                              params[0], highlighter_params);
        }

    }
    catch (runtime_error& err)
    {
        NCurses::print_status("error: " + err.description());
    }
}

void rm_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 1)
        throw wrong_argument_count();
    try
    {
        if (params[0] == "-group")
        {
            if (params.size() != 3)
                throw wrong_argument_count();
            HighlighterGroup& group = context.window().highlighters().get_group(params[1]);
            group.remove(params[2]);
        }
        else
        {
            if (params.size() != 1)
                throw wrong_argument_count();
            context.window().highlighters().remove(params[0]);
        }
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

void eval_string(const CommandParameters& params,
                 const Context& context)
{
    CommandManager::instance().execute(params, context);
}

void menu(const CommandParameters& params,
          const Context& context)
{
    if (params.size() == 0 or (params.size() % 2) != 0)
        throw wrong_argument_count();

    std::ostringstream oss;
    for (int i = 0; i < params.size(); i += 2)
    {
        oss << i/2 + 1 << "[" << params[i] << "] ";
    }
    oss << "(empty cancels): ";

    String choice = prompt_func(oss.str(), complete_nothing);
    int i = atoi(choice.c_str());

    if (i > 0 and i < (params.size() / 2) + 1)
        CommandManager::instance().execute(params[(i-1)*2+1], context);
}

}

void register_commands()
{
    CommandManager& cm = CommandManager::instance();

    cm.register_commands({ "e", "edit" }, edit<false>, CommandManager::None,
                         PerArgumentCommandCompleter({ complete_filename }));
    cm.register_commands({ "e!", "edit!" }, edit<true>, CommandManager::None,
                         PerArgumentCommandCompleter({ complete_filename }));
    cm.register_commands({ "q", "quit" }, quit<false>);
    cm.register_commands({ "q!", "quit!" }, quit<true>);
    cm.register_commands({ "w", "write" }, write_buffer,
                         CommandManager::None,
                         PerArgumentCommandCompleter({ complete_filename }));
    cm.register_command("wq", write_and_quit<false>);
    cm.register_command("wq!", write_and_quit<true>);
    cm.register_commands({ "b", "buffer" }, show_buffer,
                         CommandManager::None,
                         PerArgumentCommandCompleter({
                             [](const String& prefix, size_t cursor_pos)
                             { return BufferManager::instance().complete_buffername(prefix, cursor_pos); }
                          }));
    cm.register_commands({ "ah", "addhl" }, add_highlighter,
                         CommandManager::None,
                         [](const CommandParameters& params, size_t token_to_complete, size_t pos_in_token)
                         {
                             Window& w = main_context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                      params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.highlighters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 0 or token_to_complete == 2 and params[0] == "-group")
                                 return HighlighterRegistry::instance().complete_highlighter(arg, pos_in_token);
                             else
                                 return CandidateList();
                         });
    cm.register_commands({ "rh", "rmhl" }, rm_highlighter,
                         CommandManager::None,
                         [](const CommandParameters& params, size_t token_to_complete, size_t pos_in_token)
                         {
                             Window& w = main_context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                      params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.highlighters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 2 and params[0] == "-group")
                                 return w.highlighters().get_group(params[1]).complete_id(arg, pos_in_token);
                             else
                                 return w.highlighters().complete_id(arg, pos_in_token);
                         });
    cm.register_commands({ "af", "addfilter" }, add_filter,
                         CommandManager::None,
                         PerArgumentCommandCompleter({
                             [](const String& prefix, size_t cursor_pos)
                             { return FilterRegistry::instance().complete_filter(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "rf", "rmfilter" }, rm_filter,
                         CommandManager::None,
                         PerArgumentCommandCompleter({
                             [](const String& prefix, size_t cursor_pos)
                             { return main_context.window().complete_filterid(prefix, cursor_pos); }
                         }));
    cm.register_command("hook", add_hook, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);

    cm.register_command("source", exec_commands_in_file,
                         CommandManager::None,
                         PerArgumentCommandCompleter({ complete_filename }));
    cm.register_command("runtime", exec_commands_in_runtime_file);

    cm.register_command("exec", exec_string);
    cm.register_command("eval", eval_string);
    cm.register_command("menu", menu);

    cm.register_command("def",  define_command, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);
    cm.register_command("echo", echo_message);

    cm.register_commands({ "setg", "setglobal" },
                         [](const CommandParameters& params, const Context&)
                         { set_option(GlobalOptionManager::instance(), params); },
                         CommandManager::None,
                         PerArgumentCommandCompleter({
                             [](const String& prefix, size_t cursor_pos)
                             { return GlobalOptionManager::instance().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setb", "setbuffer" },
                         [](const CommandParameters& params, const Context& context)
                         { set_option(context.buffer().option_manager(), params); },
                         CommandManager::None,
                         PerArgumentCommandCompleter({
                             [](const String& prefix, size_t cursor_pos)
                             { return main_context.buffer().option_manager().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setw", "setwindow" },
                         [](const CommandParameters& params, const Context& context)
                         { set_option(context.window().option_manager(), params); },
                         CommandManager::None,
                         PerArgumentCommandCompleter({
                             [](const String& prefix, size_t cursor_pos)
                             { return main_context.window().option_manager().complete_option_name(prefix, cursor_pos); }
                         }));
}

}
