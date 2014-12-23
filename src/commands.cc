#include "commands.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "completion.hh"
#include "containers.hh"
#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "highlighter.hh"
#include "highlighters.hh"
#include "option_manager.hh"
#include "option_types.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "user_interface.hh"
#include "window.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace Kakoune
{

namespace
{

Buffer* open_fifo(StringView name, StringView filename, bool scroll)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0)
       throw runtime_error("unable to open " + filename);

    return create_fifo_buffer(name, fd, scroll);
}

template<typename T>
CandidateList prefix_complete(StringView prefix, const T& options)
{
    using std::begin; using std::end;
    CandidateList res;
    std::copy_if(begin(options), end(options), std::back_inserter(res),
                 std::bind(prefix_match, std::placeholders::_1, prefix));
    return res;
}

const PerArgumentCommandCompleter filename_completer({
     [](const Context& context, CompletionFlags flags, const String& prefix, ByteCount cursor_pos)
     { return Completions{ 0_byte, cursor_pos,
                           complete_filename(prefix,
                                             context.options()["ignored_files"].get<Regex>(),
                                             cursor_pos) }; }
});

const PerArgumentCommandCompleter buffer_completer({
    [](const Context& context, CompletionFlags flags, const String& prefix, ByteCount cursor_pos)
    { return Completions{ 0_byte, cursor_pos,
                          BufferManager::instance().complete_buffer_name(prefix, cursor_pos) }; }
});

const ParameterDesc no_params{
    SwitchMap{}, ParameterDesc::Flags::None, 0, 0
};

const ParameterDesc single_name_param{
    SwitchMap{}, ParameterDesc::Flags::None, 1, 1
};

const ParameterDesc single_optional_name_param{
    SwitchMap{}, ParameterDesc::Flags::None, 0, 1
};

static constexpr auto scopes = { "global", "buffer", "window" };

Scope* get_scope_ifp(StringView scope, const Context& context)
{
    if (prefix_match("global", scope))
        return &GlobalScope::instance();
    else if (prefix_match("buffer", scope))
        return &context.buffer();
    else if (prefix_match("window", scope))
        return &context.window();
    else if (prefix_match(scope, "buffer="))
        return &BufferManager::instance().get_buffer(scope.substr(7_byte));
    return nullptr;
}

Scope& get_scope(StringView scope, const Context& context)
{
    if (auto s = get_scope_ifp(scope, context))
        return *s;
    throw runtime_error("error: no such scope " + scope);
}

struct CommandDesc
{
    const char* name;
    const char* alias;
    const char* docstring;
    ParameterDesc params;
    CommandFlags flags;
    CommandCompleter completer;
    void (*func)(const ParametersParser&, Context&);
};

template<bool force_reload>
void edit(const ParametersParser& parser, Context& context)
{
    if (parser.positional_count() == 0 and not force_reload)
        throw wrong_argument_count();

    auto& name = parser.positional_count() > 0 ? parser[0]
                                               : context.buffer().name();

    Buffer* buffer = nullptr;
    if (not force_reload)
        buffer = BufferManager::instance().get_buffer_ifp(name);
    if (not buffer)
    {
        if (parser.has_option("scratch"))
        {
            BufferManager::instance().delete_buffer_if_exists(name);
            buffer = new Buffer(name, Buffer::Flags::None);
        }
        else if (parser.has_option("fifo"))
            buffer = open_fifo(name, parser.option_value("fifo"), parser.has_option("scroll"));
        else
        {
            buffer = create_buffer_from_file(name);
            if (not buffer)
            {
                if (parser.has_option("existing"))
                    throw runtime_error("unable to open " + name);

                context.print_status({ "new file " + name, get_face("StatusLine") });
                buffer = new Buffer(name, Buffer::Flags::File | Buffer::Flags::New);
            }
        }
    }

    BufferManager::instance().set_last_used_buffer(*buffer);

    const size_t param_count = parser.positional_count();
    if (buffer != &context.buffer() or param_count > 1)
        context.push_jump();

    if (buffer != &context.buffer())
        context.change_buffer(*buffer);

    if (param_count > 1 and not parser[1].empty())
    {
        int line = std::max(0, str_to_int(parser[1]) - 1);
        int column = param_count > 2 and not parser[2].empty() ?
                     std::max(0, str_to_int(parser[2]) - 1) : 0;

        auto& buffer = context.buffer();
        context.selections() = { buffer, buffer.clamp({ line,  column }) };
        if (context.has_window())
            context.window().center_line(context.selections().main().cursor().line);
    }
}

ParameterDesc edit_params{
    SwitchMap{ { "existing", { false, "fail if the file does not exists, do not open a new file" } },
               { "scratch", { false, "create a scratch buffer, not linked to a file" } },
               { "fifo", { true, "create a buffer reading its content from a named fifo" } },
               { "scroll", { false, "place the initial cursor so that the fifo will scroll to show new data" } } },
    ParameterDesc::Flags::None, 0, 3
};

const CommandDesc edit_cmd = {
    "edit",
    "e",
    "edit <switches> <filename>: open the given filename in a buffer",
    edit_params,
    CommandFlags::None,
    filename_completer,
    edit<false>
};

const CommandDesc force_edit_cmd = {
    "edit!",
    "e!",
    "edit! <switches> <filename>: open the given filename in a buffer, force reload if needed",
    edit_params,
    CommandFlags::None,
    filename_completer,
    edit<true>
};

void write_buffer(const ParametersParser& parser, Context& context)
{
    Buffer& buffer = context.buffer();

    if (parser.positional_count() == 0 and !(buffer.flags() & Buffer::Flags::File))
        throw runtime_error("cannot write a non file buffer without a filename");

    String filename = parser.positional_count() == 0 ? buffer.name()
                                     : parse_filename(parser[0]);

    write_buffer_to_file(buffer, filename);
}

const CommandDesc write_cmd = {
    "write",
    "w",
    "write [filename]: write the current buffer to it's file or to [filename] if specified",
    single_optional_name_param,
    CommandFlags::None,
    filename_completer,
    write_buffer,
};

void write_all_buffers()
{
    for (auto& buffer : BufferManager::instance())
    {
        if ((buffer->flags() & Buffer::Flags::File) and buffer->is_modified())
            write_buffer_to_file(*buffer, buffer->name());
    }
}

const CommandDesc writeall_cmd = {
    "writeall",
    "wa",
    "write all buffers that are associated to a file",
    no_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser&, Context&){ write_all_buffers(); }
};

template<bool force>
void quit()
{
    if (not force and ClientManager::instance().count() == 1)
    {
        std::vector<String> names;
        for (auto& buffer : BufferManager::instance())
        {
            if ((buffer->flags() & Buffer::Flags::File) and buffer->is_modified())
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
    // unwind back to this client event handler.
    throw client_removed{};
}

const CommandDesc quit_cmd = {
    "quit",
    "q",
    "quit current client, and the kakoune session if the client is the last (if not running in daemon mode)",
    no_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser&, Context&){ quit<false>(); }
};

const CommandDesc force_quit_cmd = {
    "quit!",
    "q!",
    "quit current client, and the kakoune session if the client is the last (if not running in daemon mode)\n"
    "force quit even if the client is the last and some buffers are not saved.",
    no_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser&, Context&){ quit<true>(); }
};

const CommandDesc write_quit_cmd = {
    "wq",
    nullptr,
    "write current buffer and quit current client",
    no_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        write_buffer(parser, context);
        quit<false>();
    }
};

const CommandDesc force_write_quit_cmd = {
    "wq!",
    nullptr,
    "write current buffer and quit current client, even if other buffers are not saved",
    no_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        write_buffer(parser, context);
        quit<true>();
    }
};

const CommandDesc writeall_quit_cmd = {
    "waq",
    nullptr,
    "write all buffers associated to a file and quit current client",
    no_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        write_all_buffers();
        quit<false>();
    }
};

const CommandDesc buffer_cmd = {
    "buffer",
    "b",
    "buffer <name>: set buffer to edit in current client",
    single_name_param,
    CommandFlags::None,
    buffer_completer,
    [](const ParametersParser& parser, Context& context)
    {
        Buffer& buffer = BufferManager::instance().get_buffer(parser[0]);
        BufferManager::instance().set_last_used_buffer(buffer);

        if (&buffer != &context.buffer())
        {
            context.push_jump();
            context.change_buffer(buffer);
        }
    }
};

template<bool force>
void delete_buffer(const ParametersParser& parser, Context& context)
{
    BufferManager& manager = BufferManager::instance();
    Buffer& buffer = parser.positional_count() == 0 ? context.buffer() : manager.get_buffer(parser[0]);
    if (not force and (buffer.flags() & Buffer::Flags::File) and buffer.is_modified())
        throw runtime_error("buffer " + buffer.name() + " is modified");

    if (manager.count() == 1)
        throw runtime_error("buffer " + buffer.name() + " is the last one");

    manager.delete_buffer(buffer);
}

const CommandDesc delbuf_cmd = {
    "delbuf",
    "db",
    "delbuf [name]: delete the current buffer or the buffer named <name> if given",
    single_optional_name_param,
    CommandFlags::None,
    buffer_completer,
    delete_buffer<false>
};

const CommandDesc force_delbuf_cmd = {
    "delbuf!",
    "db!",
    "delbuf! [name]: delete the current buffer or the buffer named <name> if given, even if the buffer is unsaved",
    single_optional_name_param,
    CommandFlags::None,
    buffer_completer,
    delete_buffer<true>
};

const CommandDesc namebuf_cmd = {
    "namebuf",
    nullptr,
    "namebuf <name>: change current buffer name",
    single_name_param,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        if (not context.buffer().set_name(parser[0]))
            throw runtime_error("unable to change buffer name to " + parser[0]);
    }
};

Completions complete_highlighter(const Context& context,
                                 StringView arg, ByteCount pos_in_token, bool only_group)
{
    const bool shared = not arg.empty() and arg[0] == '/';
    if (shared)
    {
        auto& group = DefinedHighlighters::instance();
        return offset_pos(group.complete_child(arg.substr(1_byte), pos_in_token-1, only_group), 1);
    }
    else
    {
        auto& group = context.window().highlighters();
        return group.complete_child(arg, pos_in_token, only_group);
    }
}

Completions rm_highlighter_completer(
    const Context& context, CompletionFlags flags, CommandParameters params,
    size_t token_to_complete, ByteCount pos_in_token)
{
    const String& arg = params[token_to_complete];
    if (token_to_complete == 0 and not arg.empty() and arg.front() == '/')
    {
        auto& group = DefinedHighlighters::instance();
        return offset_pos(group.complete_child(arg.substr(1_byte), pos_in_token-1, false), 1);
    }
    else if (token_to_complete == 0)
        return context.window().highlighters().complete_child(arg, pos_in_token, false);
    return {};
}

Completions add_highlighter_completer(
    const Context& context, CompletionFlags flags, CommandParameters params,
    size_t token_to_complete, ByteCount pos_in_token)
{
    StringView arg = params[token_to_complete];
    if (token_to_complete == 1 and params[0] == "-group")
        return complete_highlighter(context, params[1], pos_in_token, true);
    else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
        return { 0_byte, arg.length(), HighlighterRegistry::instance().complete_name(arg, pos_in_token) };
    return Completions{};
}

Highlighter& get_highlighter(const Context& context, StringView path)
{
    if (path.empty())
        throw runtime_error("group path should not be empty");

    Highlighter* root = nullptr;
    if (path[0] == '/')
    {
        root = &DefinedHighlighters::instance();
        path = path.substr(1_byte);
    }
    else
        root = &context.window().highlighters();

    if (path.back() == '/')
        path = path.substr(0_byte, path.length() - 1);

    if (not path.empty())
        return root->get_child(path);
    return *root;
}

const CommandDesc add_highlighter_cmd = {
    "addhl",
    "ah",
    "addhl <scope>/<group>/<id> <type> <type params>...: add an highlighter\n"
    "<scope> can be shared or window",
    ParameterDesc{
        SwitchMap{ { "group", { true, "specify the group in which to put the highlighter" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 1 },
    CommandFlags::None,
    add_highlighter_completer,
    [](const ParametersParser& parser, Context& context)
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        auto begin = parser.begin();
        const String& name = *begin++;
        std::vector<String> highlighter_params;
        for (; begin != parser.end(); ++begin)
            highlighter_params.push_back(*begin);

        auto& group = (parser.has_option("group")) ?
            get_highlighter(context, parser.option_value("group"))
          : context.window().highlighters();
        group.add_child(registry[name](highlighter_params));
    }
};

const CommandDesc rm_highlighter_cmd = {
    "rmhl",
    "rh",
    "rmhl <path>: remove highlighter <name>",
    ParameterDesc{
        SwitchMap{},
        ParameterDesc::Flags::None, 1, 1
    },
    CommandFlags::None,
    rm_highlighter_completer,
    [](const ParametersParser& parser, Context& context)
    {
        StringView path = parser[0];
        auto sep_it = find(reversed(path), '/');
        auto& group = sep_it != path.rend() ?
            get_highlighter(context, {path.begin(), sep_it.base()-1})
          : context.window().highlighters();

        group.remove_child({sep_it.base(), path.end()});
    }
};

const CommandDesc add_hook_cmd = {
    "hook",
    nullptr,
    "hook <switches> <scope> <hook_name> <command>: add <command> in <scope> to be executed on hook <hook_name>\n"
    "scope can be: \n"
    "  * global: hook is executed for any buffer or window\n"
    "  * buffer: hook is executed only for the current buffer\n"
    "            (and any window for that buffer)\n"
    "  * window: hook is executed only for the current window\n",
    ParameterDesc{
        SwitchMap{ { "group", { true, "set hook group, see rmhooks" } } },
        ParameterDesc::Flags::None, 4, 4
    },
    CommandFlags::None,
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return { 0_byte, params[0].length(),
                     prefix_complete(params[0].substr(0_byte, pos_in_token), scopes) };
        else if (token_to_complete == 3)
        {
            auto& cm = CommandManager::instance();
            return cm.complete(context, flags, params[3], pos_in_token);
        }
        return {};
    },
    [](const ParametersParser& parser, Context& context)
    {
        // copy so that the lambda gets a copy as well
        Regex regex(parser[2].begin(), parser[2].end());
        String command = parser[3];
        auto hook_func = [=](StringView param, Context& context) {
            if (context.user_hooks_support().is_disabled())
                return;

            // Do not let hooks touch prompt history
            ScopedDisable disable_history{context.history_support()};

            if (regex_match(param.begin(), param.end(), regex))
                CommandManager::instance().execute(command, context, {},
                                                   { { "hook_param", param } });
        };
        StringView group;
        if (parser.has_option("group"))
            group = parser.option_value("group");
        get_scope(parser[0], context).hooks().add_hook(parser[1], group, hook_func);
    }
};

const CommandDesc rm_hook_cmd = {
    "rmhooks",
    nullptr,
    "rmhooks <scope> <group>: remove all hooks whose group is <group>",
    ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::None, 2, 2 },
    CommandFlags::None,
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return { 0_byte, params[0].length(),
                     prefix_complete(params[0].substr(0_byte, pos_in_token), scopes) };
        else if (token_to_complete == 1)
        {
            if (auto scope = get_scope_ifp(params[0], context))
                return { 0_byte, params[0].length(),
                         scope->hooks().complete_hook_group(params[1], pos_in_token) };
        }
        return {};
    },
    [](const ParametersParser& parser, Context& context)
    {
        get_scope(parser[0], context).hooks().remove_hooks(parser[1]);
    }
};

std::vector<String> params_to_shell(const ParametersParser& parser)
{
    std::vector<String> vars;
    for (size_t i = 0; i < parser.positional_count(); ++i)
        vars.push_back(parser[i]);
    return vars;
}

void define_command(const ParametersParser& parser, Context& context)
{
    auto begin = parser.begin();
    const String& cmd_name = *begin;

    if (CommandManager::instance().command_defined(cmd_name) and
        not parser.has_option("allow-override"))
        throw runtime_error("command '" + cmd_name + "' already defined");

    CommandFlags flags = CommandFlags::None;
    if (parser.has_option("hidden"))
        flags = CommandFlags::Hidden;

    String docstring;
    if (parser.has_option("docstring"))
        docstring = parser.option_value("docstring");

    String commands = parser[1];
    Command cmd;
    ParameterDesc desc;
    if (parser.has_option("shell-params"))
    {
        desc = ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::SwitchesAsPositional };
        cmd = [=](const ParametersParser& parser, Context& context) {
            CommandManager::instance().execute(commands, context, params_to_shell(parser));
        };
    }
    else
    {
        desc = ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::SwitchesAsPositional, 0, 0 };
        cmd = [=](const ParametersParser& parser, Context& context) {
            CommandManager::instance().execute(commands, context);
        };
    }

    CommandCompleter completer;
    if (parser.has_option("file-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = params[token_to_complete];
             auto& ignored_files = context.options()["ignored_files"].get<Regex>();
             return Completions{ 0_byte, prefix.length(),
                                 complete_filename(prefix, ignored_files,
                                                   pos_in_token) };
        };
    }
    if (parser.has_option("client-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = params[token_to_complete];
             auto& cm = ClientManager::instance();
             return Completions{ 0_byte, prefix.length(),
                                 cm.complete_client_name(prefix, pos_in_token) };
        };
    }
    if (parser.has_option("buffer-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = params[token_to_complete];
             auto& bm = BufferManager::instance();
             return Completions{ 0_byte, prefix.length(),
                                 bm.complete_buffer_name(prefix, pos_in_token) };
        };
    }
    else if (parser.has_option("shell-completion"))
    {
        String shell_cmd = parser.option_value("shell-completion");
        completer = [=](const Context& context, CompletionFlags flags,
                        CommandParameters params,
                        size_t token_to_complete, ByteCount pos_in_token)
        {
            if (flags == CompletionFlags::Fast) // no shell on fast completion
                return Completions{};
            EnvVarMap vars = {
                { "token_to_complete", to_string(token_to_complete) },
                { "pos_in_token",      to_string(pos_in_token) }
            };
            String output = ShellManager::instance().eval(shell_cmd, context, params, vars);
            return Completions{ 0_byte, params[token_to_complete].length(), split(output, '\n', 0) };
        };
    }

    auto& cm = CommandManager::instance();
    cm.register_command(cmd_name, cmd, std::move(docstring), desc, flags, completer);
}

const CommandDesc define_command_cmd = {
    "def",
    nullptr,
    "def <switches> <name> <commands>: define a command named <name> corresponding to <commands>",
    ParameterDesc{
        SwitchMap{ { "shell-params", { false, "pass parameters to each shell escape as $0..$N" } },
                   { "allow-override", { false, "allow overriding an existing command" } },
                   { "hidden", { false, "do not display the command in completion candidates" } },
                   { "docstring", { true, "define the documentation string for command" } },
                   { "file-completion", { false, "complete parameters using filename completion" } },
                   { "client-completion", { false, "complete parameters using client name completion" } },
                   { "buffer-completion", { false, "complete parameters using buffer name completion" } },
                   { "shell-completion", { true, "complete the parameters using the given shell-script" } } },
        ParameterDesc::Flags::None,
        2, 2
    },
    CommandFlags::None,
    CommandCompleter{},
    define_command
};

const CommandDesc alias_cmd = {
    "alias",
    nullptr,
    "alias <scope> <alias> <command>: define a command alias in given scope",
    ParameterDesc{SwitchMap{}, ParameterDesc::Flags::None, 3, 3},
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        AliasRegistry& aliases = get_scope(parser[0], context).aliases();
        aliases.add_alias(parser[1], parser[2]);
    }
};

const CommandDesc unalias_cmd = {
    "unalias",
    nullptr,
    "unalias <scope> <alias> [<expected>]: remove a command alias in given scope\n"
    "if <expected> is specified, the alias is removed only if its value is <expected>",
    ParameterDesc{SwitchMap{}, ParameterDesc::Flags::None, 2, 3},
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        AliasRegistry& aliases = get_scope(parser[0], context).aliases();
        if (parser.positional_count() == 3 and
            aliases[parser[1]] != parser[2])
            return;
        aliases.remove_alias(parser[1]);
    }
};

const CommandDesc echo_cmd = {
    "echo",
    nullptr,
    "echo <params>...: display given parameters in the status line",
    ParameterDesc{
        SwitchMap{ { "color", { true, "set message color" } },
                   { "debug", { false, "write to debug buffer instead of status line" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart
    },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        String message;
        for (auto& param : parser)
            message += param + " ";
        if (parser.has_option("debug"))
            write_debug(message);
        else
        {
            auto face = get_face(parser.has_option("color") ?
                                 parser.option_value("color") : "StatusLine");
            context.print_status({ std::move(message), face } );
        }
    }
};


const CommandDesc debug_cmd = {
    "debug",
    nullptr,
    "debug <command>: write some debug informations in the debug buffer\n"
    "    existing commands: info, buffers",
    ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::SwitchesOnlyAtStart, 1 },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        if (parser[0] == "info")
        {
            write_debug("pid: " + to_string(getpid()));
            write_debug("session: " + Server::instance().session());
        }
        else if (parser[0] == "buffers")
        {
            write_debug("Buffers:");
            for (auto& buffer : BufferManager::instance())
                write_debug(buffer->debug_description());
        }
        else if (parser[0] == "options")
        {
            write_debug("Options:");
            for (auto& option : context.options().flatten_options())
                write_debug(" * " + option->name() + ": " + option->get_as_string());
        }
        else
            throw runtime_error("unknown debug command '" + parser[0] + "'");
    }
};

const CommandDesc source_cmd = {
    "source",
    nullptr,
    "source <filename>: execute commands contained in <filename>",
    single_name_param,
    CommandFlags::None,
    filename_completer,
    [](const ParametersParser& parser, Context& context)
    {
        String file_content = read_file(parse_filename(parser[0]));
        try
        {
            CommandManager::instance().execute(file_content, context);
        }
        catch (Kakoune::runtime_error& err)
        {
            write_debug(parser[0] + ":" + err.what());
            throw;
        }
    }
};

const CommandDesc set_option_cmd = {
    "set",
    nullptr,
    "set <switches> <scope> <name> <value>: set option <name> in <scope> to <value>",
    ParameterDesc{
        SwitchMap{ { "add", { false, "add to option rather than replacing it" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart,
        3, 3
    },
    CommandFlags::None,
    [](const Context& context, CompletionFlags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return { 0_byte, params[0].length(),
                     prefix_complete(params[0].substr(0_byte, pos_in_token), scopes) };
        else if (token_to_complete == 1)
        {
            OptionManager& options = get_scope(params[0], context).options();
            return { 0_byte, params[1].length(),
                     options.complete_option_name(params[1], pos_in_token) };
        }
        else if (token_to_complete == 2 and
                 GlobalScope::instance().option_registry().option_exists(params[1]))
        {
            OptionManager& options = get_scope(params[0], context).options();
            String val = options[params[1]].get_as_string();
            if (prefix_match(val, params[2]))
                return { 0_byte, params[2].length(), { std::move(val) } };
        }
        return Completions{};
    },
    [](const ParametersParser& parser, Context& context)
    {
        Option& opt = get_scope(parser[0], context).options().get_local_option(parser[1]);
        if (parser.has_option("add"))
            opt.add_from_string(parser[2]);
        else
            opt.set_from_string(parser[2]);
    }
};

const CommandDesc declare_option_cmd = {
    "decl",
    nullptr,
    "decl <type> <name> [value]: declare option <name> of type <type>.\n"
    "set its initial value to <value> if given and if the option did not exist\n"
    "Available types:\n"
    "    int: integer\n"
    "    bool: boolean (true/false or yes/no)\n"
    "    str: character string\n"
    "    regex: regular expression\n"
    "    int-list: list of integers\n"
    "    str-list: list of character strings\n"
    "    line-flag-list: list of line flags\n",
    ParameterDesc{
        SwitchMap{ { "hidden", { false, "do not display option name when completing" } },
                   { "docstring", { true, "specify option description" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart,
        2, 3
    },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        Option* opt = nullptr;

        OptionFlags flags = OptionFlags::None;
        if (parser.has_option("hidden"))
            flags = OptionFlags::Hidden;

        String docstring;
        if (parser.has_option("docstring"))
            docstring = parser.option_value("docstring");

        OptionsRegistry& reg = GlobalScope::instance().option_registry();

        if (parser[0] == "int")
            opt = &reg.declare_option<int>(parser[1], docstring, 0, flags);
        else if (parser[0] == "bool")
            opt = &reg.declare_option<bool>(parser[1], docstring, 0, flags);
        else if (parser[0] == "str")
            opt = &reg.declare_option<String>(parser[1], docstring, "", flags);
        else if (parser[0] == "regex")
            opt = &reg.declare_option<Regex>(parser[1], docstring, Regex{}, flags);
        else if (parser[0] == "int-list")
            opt = &reg.declare_option<std::vector<int>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "str-list")
            opt = &reg.declare_option<std::vector<String>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "line-flag-list")
            opt = &reg.declare_option<std::vector<LineAndFlag>>(parser[1], docstring, {}, flags);
        else
            throw runtime_error("unknown type " + parser[0]);

        if (parser.positional_count() == 3)
            opt->set_from_string(parser[2]);
    }
};

KeymapMode parse_keymap_mode(const String& str)
{
    if (prefix_match("normal", str)) return KeymapMode::Normal;
    if (prefix_match("insert", str)) return KeymapMode::Insert;
    if (prefix_match("menu", str))   return KeymapMode::Menu;
    if (prefix_match("prompt", str)) return KeymapMode::Prompt;
    if (prefix_match("goto", str))   return KeymapMode::Goto;
    if (prefix_match("view", str))   return KeymapMode::View;
    if (prefix_match("user", str))   return KeymapMode::User;

    throw runtime_error("unknown keymap mode '" + str + "'");
}

const CommandDesc map_key_cmd = {
    "map",
    nullptr,
    "map <scope> <mode> <key> <keys>: map <key> to <keys> in given mode at given scope.\n"
    "Valid scopes:\n"
    "    window\n"
    "    buffer\n"
    "    global\n"
    "Valid modes:\n"
    "    normal\n"
    "    insert\n"
    "    menu\n"
    "    prompt\n"
    "    user\n",
    ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::None, 4, 4 },
    CommandFlags::None,
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return { 0_byte, params[0].length(),
                     prefix_complete(params[0].substr(0_byte, pos_in_token), scopes) };
        if (token_to_complete == 1)
        {
            constexpr const char* modes[] = { "normal", "insert", "menu", "prompt", "goto", "view" };
            return { 0_byte, params[1].length(),
                     prefix_complete(params[1].substr(0_byte, pos_in_token), modes) };
        }
        return {};
    },
    [](const ParametersParser& parser, Context& context)
    {
        KeymapManager& keymaps = get_scope(parser[0], context).keymaps();
        KeymapMode keymap_mode = parse_keymap_mode(parser[1]);

        KeyList key = parse_keys(parser[2]);
        if (key.size() != 1)
            throw runtime_error("only a single key can be mapped");

        KeyList mapping = parse_keys(parser[3]);
        keymaps.map_key(key[0], keymap_mode, std::move(mapping));
    }
};

const ParameterDesc context_wrap_params = {
    SwitchMap{ { "client", { true, "run in given client context" } },
               { "try-client", { true, "run in given client context if it exists, or else in the current one" } },
               { "buffer", { true, "run in a disposable context for each given buffer in the comma separated list argument" } },
               { "draft", { false, "run in a disposable context" } },
               { "no-hooks", { false, "disable hooks" } },
               { "with-maps", { false, "use user defined key mapping when executing keys" } },
               { "itersel", { false, "run once for each selection with that selection as the only one" } } },
    ParameterDesc::Flags::SwitchesOnlyAtStart, 1
};

template<typename T>
struct DisableOption {
    DisableOption(Context& context, const char* name)
        : m_option(context.options()[name]),
          m_prev_value(m_option.get<T>())
    { m_option.set(T{}); }

    ~DisableOption() { m_option.set(m_prev_value); }

    Option& m_option;
    T m_prev_value;
};

template<typename Func>
void context_wrap(const ParametersParser& parser, Context& context, Func func)
{
    // Disable these options to avoid costly code paths (and potential screen
    // redraws) That are useful only in interactive contexts.
    DisableOption<int> disable_autoinfo(context, "autoinfo");
    DisableOption<bool> disable_autoshowcompl(context, "autoshowcompl");
    DisableOption<bool> disable_incsearch(context, "incsearch");

    const bool disable_hooks = parser.has_option("no-hooks") or
                               context.user_hooks_support().is_disabled();
    const bool disable_keymaps = not parser.has_option("with-maps");

    ClientManager& cm = ClientManager::instance();
    if (parser.has_option("buffer"))
    {
        auto names = split(parser.option_value("buffer"), ',');
        for (auto& name : names)
        {
            Buffer& buffer = BufferManager::instance().get_buffer(name);
            InputHandler input_handler{{ buffer, Selection{} },
                                       Context::Flags::Transient};
            Context& c = input_handler.context();

            // Propagate user hooks disabled status to the temporary context
            ScopedDisable hook_disable(c.user_hooks_support(), disable_hooks);
            ScopedDisable keymaps_disable(c.keymaps_support(), disable_keymaps);

            func(parser, c);
        }
        return;
    }

    Context* real_context = &context;
    if (parser.has_option("client"))
        real_context = &cm.get_client(parser.option_value("client")).context();
    else if (parser.has_option("try-client"))
    {
        Client* client = cm.get_client_ifp(parser.option_value("try-client"));
        if (client)
            real_context = &client->context();
    }

    if (parser.has_option("draft"))
    {
        InputHandler input_handler(real_context->selections(),
                                   Context::Flags::Transient,
                                   real_context->name());
        Context& c = input_handler.context();

        // We do not want this draft context to commit undo groups if the real one is
        // going to commit the whole thing later
        if (real_context->is_editing())
            c.disable_undo_handling();

        ScopedDisable hook_disable(c.user_hooks_support(), disable_hooks);
        ScopedDisable keymaps_disable(c.keymaps_support(), disable_keymaps);

        if (parser.has_option("itersel"))
        {
            SelectionList sels{real_context->selections()};
            ScopedEdition edition{c};
            for (auto& sel : sels)
            {
                c.selections() = SelectionList{ sels.buffer(), sel, sels.timestamp() };
                c.selections().update();

                func(parser, c);

                if (&sels.buffer() != &c.buffer())
                    throw runtime_error("the buffer has changed while iterating on selections");
            }
        }
        else
            func(parser, c);
    }
    else
    {
        if (parser.has_option("itersel"))
            throw runtime_error("-itersel makes no sense without -draft");

        ScopedDisable hook_disable(real_context->user_hooks_support(), disable_hooks);
        ScopedDisable keymaps_disable(real_context->keymaps_support(), disable_keymaps);

        func(parser, *real_context);
    }

    // force redraw of this client window
    if (real_context != &context and real_context->has_window())
        real_context->window().forget_timestamp();
}

const CommandDesc exec_string_cmd = {
    "exec",
    nullptr,
    "exec <switches> <keys>: execute given keys as if entered by user",
    context_wrap_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        context_wrap(parser, context, [](const ParametersParser& parser, Context& context) {
            KeyList keys;
            for (auto& param : parser)
            {
                KeyList param_keys = parse_keys(param);
                keys.insert(keys.end(), param_keys.begin(), param_keys.end());
            }
            exec_keys(keys, context);
        });
    }
};

const CommandDesc eval_string_cmd = {
    "eval",
    nullptr,
    "eval <switches> <commands>...: execute commands as if entered by user",
    context_wrap_params,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        context_wrap(parser, context, [](const ParametersParser& parser, Context& context) {
            String command;
            for (auto& param : parser)
                command += param + " ";
            CommandManager::instance().execute(command, context);
        });
    }
};

const CommandDesc prompt_cmd = {
    "prompt",
    nullptr,
    "prompt <prompt> <register> <command>: prompt the use to enter a text string "
    "stores it in <register> and then executes <command>",
    ParameterDesc{
        SwitchMap{ { "init", { true, "set initial prompt content" } } },
        ParameterDesc::Flags::None, 3, 3
    },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& params, Context& context)
    {
        if (params[1].length() != 1)
            throw runtime_error("register name should be a single character");
        const char reg = params[1][0];
        const String& command = params[2];

        String initstr;
        if (params.has_option("init"))
            initstr = params.option_value("init");

        context.input_handler().prompt(
            params[0], std::move(initstr), get_face("Prompt"), Completer{},
            [=](StringView str, PromptEvent event, Context& context)
            {
                if (event != PromptEvent::Validate)
                    return;
                RegisterManager::instance()[reg] = memoryview<String>(str);

                CommandManager::instance().execute(command, context);
            });
    }
};

const CommandDesc menu_cmd = {
    "menu",
    nullptr,
    "menu <switches> <name1> <commands1> <name2> <commands2>...: display a menu and execute commands for the selected item",
    ParameterDesc{
        SwitchMap{ { "auto-single", { false, "instantly validate if only one item is available" } },
                   { "select-cmds", { false, "each item specify an additional command to run when selected" } } }
    },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        const bool with_select_cmds = parser.has_option("select-cmds");
        const size_t modulo = with_select_cmds ? 3 : 2;

        const size_t count = parser.positional_count();
        if (count == 0 or (count % modulo) != 0)
            throw wrong_argument_count();

        if (count == modulo and parser.has_option("auto-single"))
        {
            CommandManager::instance().execute(parser[1], context);
            return;
        }

        std::vector<String> choices;
        std::vector<String> commands;
        std::vector<String> select_cmds;
        for (int i = 0; i < count; i += modulo)
        {
            choices.push_back(parser[i]);
            commands.push_back(parser[i+1]);
            if (with_select_cmds)
                select_cmds.push_back(parser[i+2]);
        }

        context.input_handler().menu(choices,
            [=](int choice, MenuEvent event, Context& context) {
                if (event == MenuEvent::Validate and choice >= 0 and choice < commands.size())
                  CommandManager::instance().execute(commands[choice], context);
                if (event == MenuEvent::Select and choice >= 0 and choice < select_cmds.size())
                  CommandManager::instance().execute(select_cmds[choice], context);
            });
    }
};

const CommandDesc info_cmd = {
    "info",
    nullptr,
    "info <switches> <params>...: display an info box with the params as content",
    ParameterDesc{
        SwitchMap{ { "anchor", { true, "set info anchoring <line>.<column>" } },
                   { "placement", { true, "set placement relative to anchor (above, below)" } },
                   { "title", { true, "set info title" } } },
        ParameterDesc::Flags::None, 0, 1
    },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        context.ui().info_hide();
        if (parser.positional_count() > 0)
        {
            InfoStyle style = InfoStyle::Prompt;
            CharCoord pos;
            if (parser.has_option("anchor"))
            {
                auto anchor = parser.option_value("anchor");
                size_t dot = anchor.find_first_of('.');
                if (dot == String::npos)
                    throw runtime_error("expected <line>.<column> for anchor");
                ByteCount dotb = (int)dot;
                ByteCoord coord{str_to_int(anchor.substr(0, dotb))-1,
                                str_to_int(anchor.substr(dotb+1))-1};
                pos = context.window().display_position(coord);
                style = InfoStyle::Inline;
                if (parser.has_option("placement"))
                {
                    auto placement = parser.option_value("placement");
                    if (placement == "above")
                        style = InfoStyle::InlineAbove;
                    else if (placement == "below")
                        style = InfoStyle::InlineBelow;
                    else
                        throw runtime_error("invalid placement " + placement);
                }
            }
            const String& title = parser.has_option("title") ? parser.option_value("title") : "";
            context.ui().info_show(title, parser[0], pos, get_face("Information"), style);
        }
    }
};

const CommandDesc try_catch_cmd = {
    "try",
    nullptr,
    "try <command> [catch <error_command>]: execute command in current context.\n"
    "if an error is raised and <error_command> is specified, execute it.\n"
    "The error is not propagated further.",
    ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::None, 1, 3 },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        if (parser.positional_count() == 2)
            throw wrong_argument_count();

        const bool do_catch = parser.positional_count() == 3;
        if (do_catch and parser[1] != "catch")
            throw runtime_error("usage: try <commands> [catch <on error commands>]");

        CommandManager& command_manager = CommandManager::instance();
        try
        {
            command_manager.execute(parser[0], context);
        }
        catch (Kakoune::runtime_error& e)
        {
            if (do_catch)
                command_manager.execute(parser[2], context);
        }
    }
};

static Completions complete_face(const Context&, CompletionFlags flags,
                                 const String& prefix, ByteCount cursor_pos)
{
    return {0_byte, cursor_pos,
            FaceRegistry::instance().complete_alias_name(prefix, cursor_pos)};
}

const CommandDesc face_cmd = {
    "face",
    nullptr,
    "face <name> <facespec>: set face <name> to refer to <facespec>\n",
    ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::None, 2, 2 },
    CommandFlags::None,
    PerArgumentCommandCompleter({ complete_face, complete_face }),
    [](const ParametersParser& parser, Context& context)
    {
        FaceRegistry::instance().register_alias(parser[0], parser[1], true);
    }
};

const CommandDesc set_client_name_cmd = {
    "nameclient",
    "nc",
    "nameclient <name>: set current client name to <name>",
    single_name_param,
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        if (ClientManager::instance().validate_client_name(parser[0]))
            context.set_name(parser[0]);
        else if (context.name() != parser[0])
            throw runtime_error("client name '" + parser[0] + "' is not unique");
    }
};

const CommandDesc set_register_cmd = {
    "reg",
    nullptr,
    "reg <name> <value>: set register <name> to <value>",
    ParameterDesc{ SwitchMap{}, ParameterDesc::Flags::None, 2, 2 },
    CommandFlags::None,
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context)
    {
        RegisterManager::instance()[parser[0]] = memoryview<String>(parser[1]);
    }
};

const CommandDesc change_working_directory_cmd = {
    "cd",
    nullptr,
    "cd <dir>: change server working directory to <dir>",
    single_name_param,
    CommandFlags::None,
    filename_completer,
    [](const ParametersParser& parser, Context&)
    {
        if (chdir(parse_filename(parser[0]).c_str()) != 0)
            throw runtime_error("cannot change to directory " + parser[0]);
    }
};

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

}

void exec_keys(memoryview<Key> keys, Context& context)
{
    RegisterRestorer quote('"', context);
    RegisterRestorer slash('/', context);

    ScopedEdition edition(context);

    for (auto& key : keys)
        context.input_handler().handle_key(key);
}

static void register_command(CommandManager& cm, const CommandDesc& c)
{
    cm.register_command(c.name, c.func, c.docstring, c.params, c.flags, c.completer);
    if (c.alias)
        GlobalScope::instance().aliases().add_alias(c.alias, c.name);
}

void register_commands()
{
    CommandManager& cm = CommandManager::instance();

    cm.register_command("nop", [](const ParametersParser&, Context&){}, "do nothing", {});

    register_command(cm, edit_cmd);
    register_command(cm, force_edit_cmd);
    register_command(cm, write_cmd);
    register_command(cm, writeall_cmd);
    register_command(cm, writeall_quit_cmd);
    register_command(cm, quit_cmd);
    register_command(cm, force_quit_cmd);
    register_command(cm, write_quit_cmd);
    register_command(cm, force_write_quit_cmd);
    register_command(cm, buffer_cmd);
    register_command(cm, delbuf_cmd);
    register_command(cm, force_delbuf_cmd);
    register_command(cm, namebuf_cmd);
    register_command(cm, add_highlighter_cmd);
    register_command(cm, rm_highlighter_cmd);
    register_command(cm, add_hook_cmd);
    register_command(cm, rm_hook_cmd);
    register_command(cm, define_command_cmd);
    register_command(cm, alias_cmd);
    register_command(cm, unalias_cmd);
    register_command(cm, echo_cmd);
    register_command(cm, debug_cmd);
    register_command(cm, source_cmd);
    register_command(cm, set_option_cmd);
    register_command(cm, declare_option_cmd);
    register_command(cm, map_key_cmd);
    register_command(cm, exec_string_cmd);
    register_command(cm, eval_string_cmd);
    register_command(cm, prompt_cmd);
    register_command(cm, menu_cmd);
    register_command(cm, info_cmd);
    register_command(cm, try_catch_cmd);
    register_command(cm, face_cmd);
    register_command(cm, set_client_name_cmd);
    register_command(cm, set_register_cmd);
    register_command(cm, change_working_directory_cmd);
}
}
