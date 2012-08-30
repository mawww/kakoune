#include "commands.hh"

#include "command_manager.hh"
#include "buffer_manager.hh"
#include "option_manager.hh"
#include "context.hh"
#include "buffer.hh"
#include "window.hh"
#include "file.hh"
#include "client.hh"
#include "string.hh"
#include "highlighter_registry.hh"
#include "filter_registry.hh"
#include "register_manager.hh"
#include "completion.hh"
#include "shell_manager.hh"
#include "event_manager.hh"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace Kakoune
{

using namespace std::placeholders;

// berk
extern bool    quit_requested;

extern std::unordered_map<Key, std::function<void (Context& context)>> keymap;

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
        for (auto& param : m_params)
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

        const String* operator->() const
        {
            assert(m_parser.m_positional[m_index]);
            return &m_parser.m_params[m_index];
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

Buffer* open_or_create(const String& filename, Context& context)
{
    Buffer* buffer = NULL;
    try
    {
        buffer = create_buffer_from_file(filename);
    }
    catch (file_not_found& what)
    {
        context.print_status("new file " + filename);
        buffer = new Buffer(filename, Buffer::Type::NewFile);
    }
    return buffer;
}

Buffer* open_fifo(const String& name , const String& filename, Context& context)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0)
       throw runtime_error("unable to open " + filename);
    Buffer* buffer = new Buffer(name, Buffer::Type::Scratch);

    buffer->hook_manager().add_hook(
        "BufClose", [=](const String&, const Context&)
        { EventManager::instance().unwatch(fd); close(fd); }
    );

    EventManager::instance().watch(fd, [=, &context](int fd) {
         char data[512];
         ssize_t count = read(fd, data, 512);
         if (count > 0)
         {
             buffer->insert(buffer->end(), String(data, data + count));
             buffer->reset_undo_data();
             context.draw_ifn();
         }
    });

    return buffer;
}

template<bool force_reload>
void edit(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "scratch", false },
                                      { "fifo", true } });

    const size_t param_count = parser.positional_count();
    if (param_count == 0 or param_count > 3)
        throw wrong_argument_count();

    const String& name = parser[0];

    Buffer* buffer = nullptr;
    if (not force_reload)
        buffer = BufferManager::instance().get_buffer(name);
    if (not buffer)
    {
        if (parser.has_option("scratch"))
            buffer = new Buffer(name, Buffer::Type::Scratch);
        else if (parser.has_option("fifo"))
            buffer = open_fifo(name, parser.option_value("fifo"), context);
        else
            buffer = open_or_create(name, context);
    }

    Window& window = *buffer->get_or_create_window();

    if (param_count > 1)
    {
        int line = std::max(0, str_to_int(parser[1]) - 1);
        int column = param_count > 2 ?
                         std::max(0, str_to_int(parser[2]) - 1) : 0;

        window.select(window.buffer().iterator_at({ line,  column }));
        window.center_selection();
    }

    context.change_editor(window);
}

void write_buffer(const CommandParameters& params, Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    Buffer& buffer = context.buffer();

    if (params.empty() and buffer.type() == Buffer::Type::Scratch)
        throw runtime_error("cannot write scratch buffer without a filename");

    String filename = params.empty() ? buffer.name()
                                     : parse_filename(params[0]);

    write_buffer_to_file(buffer, filename);
    buffer.notify_saved();
}

void write_all_buffers(const CommandParameters& params, Context& context)
{
    if (params.size() != 0)
        throw wrong_argument_count();

    for (auto& buffer : BufferManager::instance())
    {
        if (buffer->type() != Buffer::Type::Scratch and buffer->is_modified())
        {
            write_buffer_to_file(*buffer, buffer->name());
            buffer->notify_saved();
        }
    }
}

template<bool force>
void quit(const CommandParameters& params, Context& context)
{
    if (params.size() != 0)
        throw wrong_argument_count();

    if (not force)
    {
        std::vector<String> names;
        for (auto& buffer : BufferManager::instance())
        {
            if (buffer->type() != Buffer::Type::Scratch and buffer->is_modified())
                names.push_back(buffer->name());
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
            throw runtime_error(message);
        }
    }
    quit_requested = true;
}

template<bool force>
void write_and_quit(const CommandParameters& params, Context& context)
{
    write_buffer(params, context);
    quit<force>(CommandParameters(), context);
}

void show_buffer(const CommandParameters& params, Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    const String& buffer_name = params[0];
    Buffer* buffer = BufferManager::instance().get_buffer(buffer_name);
    if (not buffer)
        throw runtime_error("buffer " + buffer_name + " does not exists");

    context.change_editor(*buffer->get_or_create_window());
}

void delete_buffer(const CommandParameters& params, Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    BufferManager& manager = BufferManager::instance();
    Buffer* buffer = nullptr;
    if (params.empty())
        buffer = &context.buffer();
    else
    {
        const String& buffer_name = params[0];
        buffer = manager.get_buffer(buffer_name);
        if (not buffer)
            throw runtime_error("buffer " + buffer_name + " does not exists");
    }
    if (buffer->type()!= Buffer::Type::Scratch and buffer->is_modified())
        throw runtime_error("buffer " + buffer->name() + " is modified");

    if (&context.buffer() == buffer)
    {
        if (manager.count() == 1)
            throw runtime_error("buffer " + buffer->name() + " is the last one");
        for (auto& buf : manager)
        {
            if (buf != buffer)
            {
               context.change_editor(*buf->get_or_create_window());
               break;
            }
        }
    }
    delete buffer;
}

void add_highlighter(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() < 1)
        throw wrong_argument_count();

    HighlighterRegistry& registry = HighlighterRegistry::instance();

    auto begin = parser.begin();
    const String& name = *begin;
    std::vector<String> highlighter_params;
    for (++begin; begin != parser.end(); ++begin)
        highlighter_params.push_back(*begin);

    Window& window = context.window();
    HighlighterGroup& group = parser.has_option("group") ?
       window.highlighters().get_group(parser.option_value("group"))
     : window.highlighters();

    registry.add_highlighter_to_group(window, group, name,
                                      highlighter_params);
}

void rm_highlighter(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() != 1)
        throw wrong_argument_count();

    Window& window = context.window();
    HighlighterGroup& group = parser.has_option("group") ?
       window.highlighters().get_group(parser.option_value("group"))
     : window.highlighters();

    group.remove(parser[0]);
}

void add_filter(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() < 1)
        throw wrong_argument_count();

    FilterRegistry& registry = FilterRegistry::instance();

    auto begin = parser.begin();
    const String& name = *begin;
    std::vector<String> filter_params;
    for (++begin; begin != parser.end(); ++begin)
        filter_params.push_back(*begin);

    Window& window = context.window();
    FilterGroup& group = parser.has_option("group") ?
       window.filters().get_group(parser.option_value("group"))
     : window.filters();

    registry.add_filter_to_group(group, name, filter_params);
}

void rm_filter(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() != 1)
        throw wrong_argument_count();

    Window& window = context.window();
    FilterGroup& group = parser.has_option("group") ?
       window.filters().get_group(parser.option_value("group"))
     : window.filters();

    group.remove(parser[0]);
}

void add_hook(const CommandParameters& params, Context& context)
{
    if (params.size() != 4)
        throw wrong_argument_count();

    // copy so that the lambda gets a copy as well
    Regex regex(params[2].begin(), params[2].end());
    String command = params[3];
    auto hook_func = [=](const String& param, const Context& context) {
        if (boost::regex_match(param.begin(), param.end(), regex))
        {
            Context new_context(context);
            CommandManager::instance().execute(command, new_context);
        }
    };

    const String& scope = params[0];
    const String& name = params[1];
    if (scope == "global")
        GlobalHookManager::instance().add_hook(name, hook_func);
    else if (scope == "buffer")
        context.buffer().hook_manager().add_hook(name, hook_func);
    else if (scope == "window")
        context.window().hook_manager().add_hook(name , hook_func);
    else
        throw runtime_error("error: no such hook container " + scope);
}

EnvVarMap params_to_env_var_map(const CommandParameters& params)
{
    std::unordered_map<String, String> vars;
    char param_name[] = "param0";
    for (size_t i = 0; i < params.size(); ++i)
    {
         param_name[sizeof(param_name) - 2] = '0' + i;
         vars[param_name] = params[i];
    }
    return vars;
}

void define_command(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params,
                            { { "env-params", false },
                              { "allow-override", false },
                              { "shell-completion", true } });

    if (parser.positional_count() != 2)
        throw wrong_argument_count();

    auto begin = parser.begin();
    const String& cmd_name = *begin;

    if (CommandManager::instance().command_defined(cmd_name) and
        not parser.has_option("allow-override"))
        throw runtime_error("command '" + cmd_name + "' already defined");

    String commands = parser[1];
    Command cmd;
    if (parser.has_option("env-params"))
    {
        cmd = [=](const CommandParameters& params, Context& context) {
            CommandManager::instance().execute(commands, context,
                                               params_to_env_var_map(params));
        };
    }
    else
    {
         cmd = [=](const CommandParameters& params, Context& context) {
             if (not params.empty())
                 throw wrong_argument_count();
            CommandManager::instance().execute(commands, context);
        };
    }

    if (parser.has_option("shell-completion"))
    {
        String shell_cmd = parser.option_value("shell-completion");
        auto completer = [=](const Context& context, const CommandParameters& params,
                             size_t token_to_complete, CharCount pos_in_token)
        {
           EnvVarMap vars = params_to_env_var_map(params);
           vars["token_to_complete"] = int_to_str(token_to_complete);
           vars["pos_in_token"]      = int_to_str((int)pos_in_token);
           String output = ShellManager::instance().eval(shell_cmd, context, vars);
           return split(output, '\n');
        };
        CommandManager::instance().register_command(cmd_name, cmd, completer);
    }
    else
        CommandManager::instance().register_command(cmd_name, cmd);
}

void echo_message(const CommandParameters& params, Context& context)
{
    String message;
    for (auto& param : params)
        message += param + " ";
    context.print_status(message);
}

void exec_commands_in_file(const CommandParameters& params,
                           Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    String file_content = read_file(parse_filename(params[0]));
    CommandManager::instance().execute(file_content, context);
}

void exec_commands_in_runtime_file(const CommandParameters& params,
                                   Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    const String& filename = params[0];
    char buffer[2048];
#if defined(__linux__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048 - (int)filename.length());
    assert(res != -1);
    buffer[res] = '\0';
#elif defined(__APPLE__)
    uint32_t bufsize = 2048 - (int)filename.length();
    _NSGetExecutablePath(buffer, &bufsize);
    char* canonical_path = realpath(buffer, NULL);
    strncpy(buffer, canonical_path, 2048 - (int)filename.length());
    free(canonical_path);
#else
# error "finding executable path is not implemented on this platform"
#endif
    char* ptr = strrchr(buffer, '/');
    if (ptr)
    {
        strcpy(ptr+1, filename.c_str());
        exec_commands_in_file(CommandParameters(buffer), context);
    }
}

void set_option(OptionManager& option_manager, const CommandParameters& params,
                Context& context)
{
    if (params.size() != 2)
        throw wrong_argument_count();

    option_manager.set_option(params[0], Option(params[1]));
}

class RegisterRestorer
{
public:
    RegisterRestorer(char name, const Context& context)
       : m_name(name)
    {
         memoryview<String> save = RegisterManager::instance()[name].values(context);
         m_save = std::vector<String>(save.begin(), save.end());
    }

    ~RegisterRestorer()
    { RegisterManager::instance()[m_name] = m_save; }

private:
    std::vector<String> m_save;
    char                m_name;
};

class BatchClient : public Client
{
public:
    BatchClient(const KeyList& keys, Client* previous_client)
        : m_keys(keys), m_pos(0)
    {
        m_previous_client = previous_client;
    }

    String prompt(const String&, const Context&, Completer)
    {
        size_t begin = m_pos;
        while (m_pos < m_keys.size() and m_keys[m_pos].key != '\n')
            ++m_pos;

        String result;
        for (size_t i = begin; i < m_pos; ++i)
            result += String() + m_keys[i].key;
        ++m_pos;

        return result;
    }

    Key get_key()
    {
        if (m_pos >= m_keys.size())
            throw runtime_error("no more characters");
        return m_keys[m_pos++];
    }

    void print_status(const String& status)
    {
        m_previous_client->print_status(status);
    }

    void draw_window(Window& window)
    {
        m_previous_client->draw_window(window);
    }

    bool has_key_left() const { return m_pos < m_keys.size(); }

    int menu(const memoryview<String>& choices) { return 0; }

private:
    const KeyList& m_keys;
    size_t         m_pos;
    Client*        m_previous_client;
};

void exec_keys(const KeyList& keys, Context& context)
{
    BatchClient batch_client(keys, context.has_client() ? &context.client()
                                                       : nullptr);

    RegisterRestorer quote('"', context);
    RegisterRestorer slash('/', context);

    scoped_edition edition(context.editor());

    int count = 0;
    Context new_context(batch_client);
    new_context.change_editor(context.editor());
    while (batch_client.has_key_left())
    {
        Key key = batch_client.get_key();

        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            count = count * 10 + key.key - '0';
        else
        {
            auto it = keymap.find(key);
            if (it != keymap.end())
            {
                new_context.numeric_param(count);
                it->second(new_context);
            }
            count = 0;
        }
    }
}

void exec_string(const CommandParameters& params, Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    KeyList keys = parse_keys(params[0]);

    exec_keys(keys, context);
}

void menu(const CommandParameters& params, Context& context)
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

    std::vector<String> choices;
    for (int i = 0; i < count; i += 2)
        choices.push_back(parser[i]);

    int i = context.client().menu(choices);

    if (i > 0 and i < (count / 2) + 1)
        CommandManager::instance().execute(parser[(i-1)*2+1], context);
}

void try_catch(const CommandParameters& params, Context& context)
{
    if (params.size() != 3)
        throw wrong_argument_count();
    if (params[1] != "catch")
        throw runtime_error("try needs a catch");

    CommandManager& command_manager = CommandManager::instance();
    try
    {
        command_manager.execute(params[0], context);
    }
    catch (Kakoune::runtime_error& e)
    {
        command_manager.execute(params[2], context);
    }
}

}

void register_commands()
{
    CommandManager& cm = CommandManager::instance();

    PerArgumentCommandCompleter filename_completer({ complete_filename });
    cm.register_commands({ "e", "edit" }, edit<false>, filename_completer);
    cm.register_commands({ "e!", "edit!" }, edit<true>, filename_completer);
    cm.register_commands({ "w", "write" }, write_buffer, filename_completer);
    cm.register_commands({ "wa", "writeall" }, write_all_buffers);
    cm.register_commands({ "q", "quit" }, quit<false>);
    cm.register_commands({ "q!", "quit!" }, quit<true>);
    cm.register_command("wq", write_and_quit<false>);
    cm.register_command("wq!", write_and_quit<true>);

    PerArgumentCommandCompleter buffer_completer({
        [](const Context& context, const String& prefix, CharCount cursor_pos)
        { return BufferManager::instance().complete_buffername(prefix, cursor_pos); }
    });
    cm.register_commands({ "b", "buffer" }, show_buffer, buffer_completer);
    cm.register_commands({ "db", "delbuf" }, delete_buffer, buffer_completer);

    cm.register_commands({ "ah", "addhl" }, add_highlighter,
                         [](const Context& context, const CommandParameters& params,
                           size_t token_to_complete, CharCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.highlighters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
                                 return HighlighterRegistry::instance().complete_highlighter(arg, pos_in_token);
                             else
                                 return CandidateList();
                         });
    cm.register_commands({ "rh", "rmhl" }, rm_highlighter,
                         [](const Context& context, const CommandParameters& params,
                            size_t token_to_complete, CharCount pos_in_token)
                         {
                             Window& w = context.window();
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
                         [](const Context& context, const CommandParameters& params,
                            size_t token_to_complete, CharCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.filters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
                                 return FilterRegistry::instance().complete_filter(arg, pos_in_token);
                             else
                                 return CandidateList();
                         });
    cm.register_commands({ "rf", "rmfilter" }, rm_filter,
                         [](const Context& context, const CommandParameters& params,
                            size_t token_to_complete, CharCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.filters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 2 and params[0] == "-group")
                                 return w.filters().get_group(params[1]).complete_id(arg, pos_in_token);
                             else
                                 return w.filters().complete_id(arg, pos_in_token);
                         });

    cm.register_command("hook", add_hook);

    cm.register_command("source", exec_commands_in_file, filename_completer);
    cm.register_command("runtime", exec_commands_in_runtime_file);

    cm.register_command("exec", exec_string);
    cm.register_command("menu", menu);
    cm.register_command("try",  try_catch);

    cm.register_command("def",  define_command);
    cm.register_command("echo", echo_message);

    cm.register_commands({ "setg", "setglobal" },
                         [](const CommandParameters& params, Context& context)
                         { set_option(GlobalOptionManager::instance(), params, context); },
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, CharCount cursor_pos)
                             { return GlobalOptionManager::instance().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setb", "setbuffer" },
                         [](const CommandParameters& params, Context& context)
                         { set_option(context.buffer().option_manager(), params, context); },
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, CharCount cursor_pos)
                             { return context.buffer().option_manager().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setw", "setwindow" },
                         [](const CommandParameters& params, Context& context)
                         { set_option(context.window().option_manager(), params, context); },
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, CharCount cursor_pos)
                             { return context.window().option_manager().complete_option_name(prefix, cursor_pos); }
                         }));
}

}
