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
#include "shell_manager.hh"


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

struct unknown_option : public runtime_error
{
    unknown_option(const String& name)
        : runtime_error("unknown option '" + name + "'") {}
};

struct missing_option_value: public runtime_error
{
    missing_option_value(const String& name)
        : runtime_error("missing value for option '" + name + "'") {}
};

// ParameterParser provides tools to parse command parameters.
// There are 3 types of parameters:
//  * unnamed options, which are accessed by position (ignoring named ones)
//  * named boolean options, which are enabled using '-name' syntax
//  * named string options,  which are defined using '-name value' syntax
struct ParametersParser
{
    // the options defines named options, if they map to true, then
    // they are understood as string options, else they are understood as
    // boolean option.
    ParametersParser(const CommandParameters& params,
                     std::unordered_map<String, bool> options)
        : m_params(params), m_positional(params.size(), true), m_options(options)
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (params[i][0] == '-')
            {
                auto it = options.find(params[i].substr(1));
                if (it == options.end())
                    throw unknown_option(params[i]);

                if (it->second)
                {
                    if (i + 1 == params.size() or params[i+1][0] == '-')
                       throw missing_option_value(params[i]);

                    m_positional[i+1] = false;
                }
                m_positional[i] = false;
            }

            // all options following -- are positional
            if (params[i] == "--")
                break;
        }
    }

    // check if a named option (either string or boolean) is specified
    bool has_option(const String& name) const
    {
        assert(m_options.find(name) != m_options.end());
        for (auto param : m_params)
        {
            if (param[0] == '-' and param.substr(1) == name)
                return true;

            if (param == "--")
                break;
        }
        return false;
    }

    // get a string option value, returns an empty string if the option
    // is not defined
    const String& option_value(const String& name) const
    {
        auto it = m_options.find(name);
        assert(it != m_options.end());
        assert(it->second == true);

        for (size_t i = 0; i < m_params.size(); ++i)
        {
            if (m_params[i][0] == '-' and m_params[i].substr(1) == name)
                return m_params[i+1];

            if (m_params[i] == "--")
                break;
        }
        static String empty;
        return empty;
    }

    size_t positional_count() const
    {
        size_t res = 0;
        for (bool positional : m_positional)
        {
           if (positional)
               ++res;
        }
        return res;
    }

    struct iterator
    {
    public:
        typedef String            value_type;
        typedef const value_type* pointer;
        typedef const value_type& reference;
        typedef size_t            difference_type;
        typedef std::forward_iterator_tag iterator_category;

        iterator(const ParametersParser& parser, size_t index)
            : m_parser(parser), m_index(index) {}

        const String& operator*() const
        {
            assert(m_parser.m_positional[m_index]);
            return m_parser.m_params[m_index];
        }

        iterator& operator++()
        {
           while (m_index < m_parser.m_positional.size() and
                  not m_parser.m_positional[++m_index]) {}
           return *this;
        }

        bool operator==(const iterator& other) const
        {
            return &m_parser == &other.m_parser and m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            return &m_parser != &other.m_parser or m_index != other.m_index;
        }

        bool operator<(const iterator& other) const
        {
            assert(&m_parser == &other.m_parser);
            return m_index < other.m_index;
        }

    private:
        const ParametersParser& m_parser;
        size_t                  m_index;
    };

    // positional parameter begin
    iterator begin() const
    {
        int index = 0;
        while (index < m_positional.size() and not m_positional[index])
            ++index;
        return iterator(*this, index);
    }

    // positional parameter end
    iterator end() const
    {
        return iterator(*this, m_params.size());
    }

    // access positional parameter by index
    const String& operator[] (size_t index) const
    {
        assert(index < positional_count());
        iterator it = begin();
        while (index)
        {
            ++it;
            --index;
        }
        return *it;
    }

private:
    const CommandParameters& m_params;
    std::vector<bool>        m_positional;
    std::unordered_map<String, bool> m_options;
};

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
        throw runtime_error("buffer " + params[0] + " does not exists");

    main_context = Context(*buffer->get_or_create_window());
}

void delete_buffer(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    BufferManager& manager = BufferManager::instance();

    Buffer* buffer = manager.get_buffer(params[0]);
    if (not buffer)
        throw runtime_error("buffer " + params[0] + " does not exists");
    if (buffer->type()!= Buffer::Type::Scratch and buffer->is_modified())
        throw runtime_error("buffer " + params[0] + " is modified");

    if (&main_context.buffer() == buffer)
    {
        if (manager.count() == 1)
            throw runtime_error("buffer " + params[0] + " is the last one");
        for (Buffer& buf : manager)
        {
            if (&buf != buffer)
            {
               main_context = Context(*buf.get_or_create_window());
               break;
            }
        }
    }
    delete buffer;
}

void add_highlighter(const CommandParameters& params, const Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() < 1)
        throw wrong_argument_count();
    try
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        auto begin = parser.begin();
        const String& name = *begin;
        std::vector<String> highlighter_params(++begin, parser.end());

        Window& window = context.window();
        HighlighterGroup& group = parser.has_option("group") ?
           window.highlighters().get_group(parser.option_value("group"))
         : window.highlighters();

        registry.add_highlighter_to_group(window, group, name,
                                          highlighter_params);
    }
    catch (runtime_error& err)
    {
        NCurses::print_status("error: " + err.description());
    }
}

void rm_highlighter(const CommandParameters& params, const Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() != 1)
        throw wrong_argument_count();
    try
    {
        Window& window = context.window();
        HighlighterGroup& group = parser.has_option("group") ?
           window.highlighters().get_group(parser.option_value("group"))
         : window.highlighters();

        group.remove(*parser.begin());
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

EnvVarMap params_to_env_var_map(const CommandParameters& params)
{
    std::unordered_map<String, String> vars;
    char param_name[] = "param0";
    for (size_t i = 0; i < params.size(); ++i)
    {
         param_name[sizeof(param_name) - 2] = '0' + i;
         vars[param_name] = params[i].c_str();
    }
    return vars;
}

void define_command(const CommandParameters& params, const Context& context)
{
    ParametersParser parser(params,
                            { { "env-params", false },
                              { "append-params", false },
                              { "allow-override", false },
                              { "shell-completion", true } });

    if (parser.positional_count() < 2)
        throw wrong_argument_count();

    auto begin = parser.begin();
    const String& cmd_name = *begin;

    if (CommandManager::instance().command_defined(cmd_name) and
        not parser.has_option("allow-override"))
        throw runtime_error("command '" + cmd_name + "' already defined");

    std::vector<String> cmd_params(++begin, parser.end());
    Command cmd;
    if (parser.has_option("env-params"))
    {
        cmd = [=](const CommandParameters& params, const Context& context) {
            CommandManager::instance().execute(cmd_params, context,
                                               params_to_env_var_map(params));
        };
    }
    else if (parser.has_option("append-params"))
    {
         cmd = [=](const CommandParameters& params, const Context& context) {
            std::vector<String> merged_params = cmd_params;
            for (auto& param : params)
                merged_params.push_back(param);
            CommandManager::instance().execute(merged_params, context);
        };
    }
    else
    {
         cmd = [=](const CommandParameters& params, const Context& context) {
             if (not params.empty())
                 throw wrong_argument_count();
            CommandManager::instance().execute(cmd_params, context);
        };
    }

    if (parser.has_option("shell-completion"))
    {
        String shell_cmd = parser.option_value("shell-completion");
        auto completer = [=](const CommandParameters& params,
                             size_t token_to_complete, size_t pos_in_token)
        {
           EnvVarMap vars = params_to_env_var_map(params);
           vars["token_to_complete"] = int_to_str(token_to_complete);
           vars["pos_in_token"]      = int_to_str(pos_in_token);
           String output = ShellManager::instance().eval(shell_cmd, context, vars);
           return split(output, '\n');
        };
        CommandManager::instance().register_command(cmd_name, cmd,
                                                    CommandManager::None,
                                                    completer);
    }
    else
        CommandManager::instance().register_command(cmd_name, cmd);
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
    ParametersParser parser(params, { { "auto-single", false } });

    size_t count = parser.positional_count();
    if (count == 0 or (count % 2) != 0)
        throw wrong_argument_count();

    if (count == 2 and parser.has_option("auto-single"))
    {
        CommandManager::instance().execute(parser[1], context);
        return;
    }

    std::ostringstream oss;
    for (int i = 0; i < count; i += 2)
    {
        oss << i/2 + 1 << "[" << parser[i] << "] ";
    }
    oss << "(empty cancels): ";

    String choice = prompt_func(oss.str(), complete_nothing);
    int i = atoi(choice.c_str());

    if (i > 0 and i < (count / 2) + 1)
        CommandManager::instance().execute(parser[(i-1)*2+1], context);
}

void try_catch(const CommandParameters& params,
               const Context& context)
{
    size_t i = 0;
    for (; i < params.size(); ++i)
    {
        if (params[i] == "catch")
            break;
    }

    if (i == 0 or i == params.size())
        throw wrong_argument_count();

    CommandManager& command_manager = CommandManager::instance();
    try
    {
        command_manager.execute(params.subrange(0, i), context);
    }
    catch (Kakoune::runtime_error& e)
    {
        command_manager.execute(params.subrange(i+1, params.size() - i - 1), context);
    }
}

}

void register_commands()
{
    CommandManager& cm = CommandManager::instance();

    PerArgumentCommandCompleter filename_completer({ complete_filename });
    cm.register_commands({ "e", "edit" }, edit<false>,
                         CommandManager::None, filename_completer);
    cm.register_commands({ "e!", "edit!" }, edit<true>,
                         CommandManager::None, filename_completer);
    cm.register_commands({ "w", "write" }, write_buffer,
                         CommandManager::None, filename_completer);
    cm.register_commands({ "q", "quit" }, quit<false>);
    cm.register_commands({ "q!", "quit!" }, quit<true>);
    cm.register_command("wq", write_and_quit<false>);
    cm.register_command("wq!", write_and_quit<true>);

    PerArgumentCommandCompleter buffer_completer({
        [](const String& prefix, size_t cursor_pos)
        { return BufferManager::instance().complete_buffername(prefix, cursor_pos); }
    });
    cm.register_commands({ "b", "buffer" }, show_buffer,
                         CommandManager::None, buffer_completer);
    cm.register_commands({ "db", "delbuf" }, delete_buffer,
                         CommandManager::None, buffer_completer);

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

    PerArgumentCommandCompleter filter_completer({
        [](const String& prefix, size_t cursor_pos)
        { return FilterRegistry::instance().complete_filter(prefix, cursor_pos); }
    });
    cm.register_commands({ "af", "addfilter" }, add_filter,
                         CommandManager::None, filter_completer);
    cm.register_commands({ "rf", "rmfilter" }, rm_filter,
                         CommandManager::None, filter_completer);

    cm.register_command("hook", add_hook, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);

    cm.register_command("source", exec_commands_in_file,
                         CommandManager::None, filename_completer);
    cm.register_command("runtime", exec_commands_in_runtime_file);

    cm.register_command("exec", exec_string);
    cm.register_command("eval", eval_string);
    cm.register_command("menu", menu);
    cm.register_command("try",  try_catch, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);

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
