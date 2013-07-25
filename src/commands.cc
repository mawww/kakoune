#include "commands.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "client_manager.hh"
#include "color_registry.hh"
#include "command_manager.hh"
#include "completion.hh"
#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "file.hh"
#include "filter.hh"
#include "highlighter.hh"
#include "highlighters.hh"
#include "input_handler.hh"
#include "option_manager.hh"
#include "option_types.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "user_interface.hh"
#include "utf8_iterator.hh"
#include "window.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace Kakoune
{

namespace
{

Buffer* open_or_create(const String& filename, Context& context)
{
    Buffer* buffer = create_buffer_from_file(filename);
    if (not buffer)
    {
        context.print_status({ "new file " + filename, get_color("StatusLine") });
        buffer = new Buffer(filename, Buffer::Flags::File | Buffer::Flags::New);
    }
    return buffer;
}

Buffer* open_fifo(const String& name , const String& filename, Context& context)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0)
       throw runtime_error("unable to open " + filename);

    BufferManager::instance().delete_buffer_if_exists(name);

    Buffer* buffer = new Buffer(name, Buffer::Flags::Fifo | Buffer::Flags::NoUndo);

    auto watcher = new FDWatcher(fd, [buffer](FDWatcher& watcher) {
        constexpr size_t buffer_size = 1024 * 16;
        char data[buffer_size];
        ssize_t count = read(watcher.fd(), data, buffer_size);
        buffer->insert(buffer->end()-1, count > 0 ? String(data, data+count)
                                                  : "*** kak: fifo closed ***\n");
        ClientManager::instance().redraw_clients();
        if (count <= 0)
        {
            kak_assert(buffer->flags() & Buffer::Flags::Fifo);
            buffer->flags() &= ~Buffer::Flags::Fifo;
            close(watcher.fd());
            delete &watcher;
        }
    });

    buffer->hooks().add_hook("BufClose", "",
        [buffer, watcher](const String&, const Context&) {
            // Check if fifo is still alive, else watcher is already dead
            if (buffer->flags() & Buffer::Flags::Fifo)
            {
                close(watcher->fd());
                delete watcher;
            }
        });

    return buffer;
}

template<bool force_reload>
void edit(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "scratch", false },
                                      { "fifo", true } },
                            ParametersParser::Flags::None, 1, 3);

    const String name = parser[0];

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
            buffer = open_fifo(name, parser.option_value("fifo"), context);
        else
            buffer = open_or_create(name, context);
    }

    BufferManager::instance().set_last_used_buffer(*buffer);

    const size_t param_count = parser.positional_count();
    if (buffer != &context.buffer() or param_count > 1)
        context.push_jump();

    if (buffer != &context.buffer())
    {
        auto& manager = ClientManager::instance();
        context.change_editor(manager.get_unused_window_for_buffer(*buffer));
    }

    if (param_count > 1)
    {
        int line = std::max(0, str_to_int(parser[1]) - 1);
        int column = param_count > 2 ?
                     std::max(0, str_to_int(parser[2]) - 1) : 0;

        context.editor().select(context.buffer().clamp({ line,  column }));
        if (context.has_window())
            context.window().center_selection();
    }
}

void write_buffer(CommandParameters params, Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    Buffer& buffer = context.buffer();

    if (params.empty() and !(buffer.flags() & Buffer::Flags::File))
        throw runtime_error("cannot write a non file buffer without a filename");

    String filename = params.empty() ? buffer.name()
                                     : parse_filename(params[0]);

    write_buffer_to_file(buffer, filename);

    if (filename == buffer.name())
        buffer.notify_saved();
}

void write_all_buffers(CommandParameters params, Context& context)
{
    if (params.size() != 0)
        throw wrong_argument_count();

    for (auto& buffer : BufferManager::instance())
    {
        if ((buffer->flags() & Buffer::Flags::File) and buffer->is_modified())
        {
            write_buffer_to_file(*buffer, buffer->name());
            buffer->notify_saved();
        }
    }
}

template<bool force>
void quit(CommandParameters params, Context& context)
{
    if (params.size() != 0)
        throw wrong_argument_count();

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

template<bool force>
void write_and_quit(CommandParameters params, Context& context)
{
    write_buffer(params, context);
    quit<force>(CommandParameters(), context);
}

void show_buffer(CommandParameters params, Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    Buffer& buffer = BufferManager::instance().get_buffer(params[0]);
    BufferManager::instance().set_last_used_buffer(buffer);

    if (&buffer != &context.buffer())
    {
        context.push_jump();
        auto& manager = ClientManager::instance();
        context.change_editor(manager.get_unused_window_for_buffer(buffer));
    }
}

template<bool force>
void delete_buffer(CommandParameters params, Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    BufferManager& manager = BufferManager::instance();
    Buffer& buffer = params.empty() ? context.buffer() : manager.get_buffer(params[0]);
    if (not force and (buffer.flags() & Buffer::Flags::File) and buffer.is_modified())
        throw runtime_error("buffer " + buffer.name() + " is modified");

    if (manager.count() == 1)
        throw runtime_error("buffer " + buffer.name() + " is the last one");

    manager.delete_buffer(buffer);
}

void set_buffer_name(CommandParameters params, Context& context)
{
    ParametersParser parser(params, OptionMap{},
                            ParametersParser::Flags::None, 1, 1);
    if (not context.buffer().set_name(parser[0]))
        throw runtime_error("unable to change buffer name to " + parser[0]);
}

template<typename Group>
Group& get_group(Group& root, const String& group_path)
{
    auto it = find(group_path, '/');
    Group& group = root.get_group(String(group_path.begin(), it));
    if (it != group_path.end())
        return get_group(group, String(it+1, group_path.end()));
    else
        return group;
}

void add_highlighter(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "group", true } }, ParametersParser::Flags::None, 1);
    HighlighterRegistry& registry = HighlighterRegistry::instance();

    auto begin = parser.begin();
    const String& name = *begin;
    std::vector<String> highlighter_params;
    for (++begin; begin != parser.end(); ++begin)
        highlighter_params.push_back(*begin);

    Window& window = context.window();
    HighlighterGroup& group = parser.has_option("group") ?
        get_group(window.highlighters(), parser.option_value("group"))
      : window.highlighters();

    auto& factory = registry[name];
    group.append(factory(highlighter_params, window));
}

void rm_highlighter(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "group", true } }, ParametersParser::Flags::None, 1, 1);

    Window& window = context.window();
    HighlighterGroup& group = parser.has_option("group") ?
        get_group(window.highlighters(), parser.option_value("group"))
      : window.highlighters();

    group.remove(parser[0]);
}

void add_filter(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "group", true } }, ParametersParser::Flags::None, 1);

    FilterRegistry& registry = FilterRegistry::instance();

    auto begin = parser.begin();
    const String& name = *begin;
    std::vector<String> filter_params;
    for (++begin; begin != parser.end(); ++begin)
        filter_params.push_back(*begin);

    Editor& editor = context.editor();
    FilterGroup& group = parser.has_option("group") ?
        get_group(editor.filters(), parser.option_value("group"))
      : editor.filters();

    auto& factory = registry[name];
    group.append(factory(filter_params));
}

void rm_filter(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "group", true } }, ParametersParser::Flags::None, 1, 1);

    Editor& editor = context.editor();
    FilterGroup& group = parser.has_option("group") ?
        get_group(editor.filters(), parser.option_value("group"))
      : editor.filters();

    group.remove(parser[0]);
}

static HookManager& get_hook_manager(const String& scope, Context& context)
{
    if (scope == "global")
        return GlobalHooks::instance();
    else if (scope == "buffer")
        return context.buffer().hooks();
    else if (scope == "window")
        return context.window().hooks();
    throw runtime_error("error: no such hook container " + scope);
}

void add_hook(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "id", true } }, ParametersParser::Flags::None, 4, 4);
    // copy so that the lambda gets a copy as well
    Regex regex(parser[2].begin(), parser[2].end());
    String command = parser[3];
    auto hook_func = [=](const String& param, Context& context) {
        if (boost::regex_match(param.begin(), param.end(), regex))
            CommandManager::instance().execute(command, context, {},
                                               { { "hook_param", param } });
    };
    String id = parser.has_option("id") ? parser.option_value("id") : "";
    get_hook_manager(parser[0], context).add_hook(parser[1], id, hook_func);
}

void rm_hooks(CommandParameters params, Context& context)
{
    ParametersParser parser(params, {}, ParametersParser::Flags::None, 2, 2);
    get_hook_manager(parser[0], context).remove_hooks(parser[1]);
}

EnvVarMap params_to_env_var_map(CommandParameters params)
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

void define_command(CommandParameters params, Context& context)
{
    ParametersParser parser(params,
                            { { "env-params", false },
                              { "shell-params", false },
                              { "allow-override", false },
                              { "file-completion", false },
                              { "shell-completion", true } },
                             ParametersParser::Flags::None,
                             2, 2);

    auto begin = parser.begin();
    const String& cmd_name = *begin;

    if (CommandManager::instance().command_defined(cmd_name) and
        not parser.has_option("allow-override"))
        throw runtime_error("command '" + cmd_name + "' already defined");

    String commands = parser[1];
    Command cmd;
    if (parser.has_option("env-params"))
    {
        cmd = [=](CommandParameters params, Context& context) {
            CommandManager::instance().execute(commands, context, {},
                                               params_to_env_var_map(params));
        };
    }
    if (parser.has_option("shell-params"))
    {
        cmd = [=](CommandParameters params, Context& context) {
            CommandManager::instance().execute(commands, context, params);
        };
    }
    else
    {
        cmd = [=](CommandParameters params, Context& context) {
            if (not params.empty())
                throw wrong_argument_count();
            CommandManager::instance().execute(commands, context);
        };
    }

    CommandCompleter completer;
    if (parser.has_option("file-completion"))
    {
        completer = [](const Context& context, CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = token_to_complete < params.size() ?
                                    params[token_to_complete] : String();
             return complete_filename(prefix, context.options()["ignored_files"].get<Regex>(), pos_in_token);
        };
    }
    else if (parser.has_option("shell-completion"))
    {
        String shell_cmd = parser.option_value("shell-completion");
        completer = [=](const Context& context, CommandParameters params,
                        size_t token_to_complete, ByteCount pos_in_token)
        {
            EnvVarMap vars = {
                { "token_to_complete", to_string(token_to_complete) },
                { "pos_in_token",      to_string(pos_in_token) }
            };
            String output = ShellManager::instance().eval(shell_cmd, context, params, vars);
            return split(output, '\n');
        };
    }
    CommandManager::instance().register_command(cmd_name, cmd, completer);
}

void echo_message(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "color", true } },
                            ParametersParser::Flags::OptionsOnlyAtStart);
    String message;
    for (auto& param : parser)
        message += param + " ";
    ColorPair color = get_color(parser.has_option("color") ?
                                parser.option_value("color") : "StatusLine");
    context.print_status({ std::move(message), color } );
}

void write_debug_message(CommandParameters params, Context&)
{
    String message;
    for (auto& param : params)
        message += param + " ";
    write_debug(message);
}

void exec_commands_in_file(CommandParameters params,
                           Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    String file_content = read_file(parse_filename(params[0]));
    CommandManager::instance().execute(file_content, context);
}

void set_global_option(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "add", false } },
                            ParametersParser::Flags::OptionsOnlyAtStart,
                            2, 2);

    Option& opt = GlobalOptions::instance().get_local_option(parser[0]);
    if (parser.has_option("add"))
        opt.add_from_string(parser[1]);
    else
        opt.set_from_string(parser[1]);
}

void set_buffer_option(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "buffer", true}, { "add", false } },
                            ParametersParser::Flags::OptionsOnlyAtStart,
                            2, 2);

    OptionManager& options = parser.has_option("buffer") ?
        BufferManager::instance().get_buffer(parser.option_value("buffer")).options()
      : context.buffer().options();

    Option& opt = options.get_local_option(parser[0]);
    if (parser.has_option("add"))
        opt.add_from_string(parser[1]);
    else
        opt.set_from_string(parser[1]);
}

void set_window_option(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "add", false } },
                            ParametersParser::Flags::OptionsOnlyAtStart,
                            2, 2);

    Option& opt = context.window().options().get_local_option(parser[0]);
    if (parser.has_option("add"))
        opt.add_from_string(parser[1]);
    else
        opt.set_from_string(parser[1]);
}

void declare_option(CommandParameters params, Context& context)
{
    if (params.size() != 2 and params.size() != 3)
        throw wrong_argument_count();
    Option* opt = nullptr;

    if (params[0] == "int")
        opt = &GlobalOptions::instance().declare_option<int>(params[1], 0);
    if (params[0] == "bool")
        opt = &GlobalOptions::instance().declare_option<bool>(params[1], 0);
    else if (params[0] == "str")
        opt = &GlobalOptions::instance().declare_option<String>(params[1], "");
    else if (params[0] == "regex")
        opt = &GlobalOptions::instance().declare_option<Regex>(params[1], Regex{});
    else if (params[0] == "int-list")
        opt = &GlobalOptions::instance().declare_option<std::vector<int>>(params[1], {});
    else if (params[0] == "str-list")
        opt = &GlobalOptions::instance().declare_option<std::vector<String>>(params[1], {});
    else if (params[0] == "line-flag-list")
        opt = &GlobalOptions::instance().declare_option<std::vector<LineAndFlag>>(params[1], {});
    else
        throw runtime_error("unknown type " + params[0]);

    if (params.size() == 3)
        opt->set_from_string(params[2]);
}

template<typename Func>
void context_wrap(CommandParameters params, Context& context, Func func)
{
    ParametersParser parser(params, { { "client", true }, { "draft", false }},
                            ParametersParser::Flags::OptionsOnlyAtStart, 1);

    Context& real_context = parser.has_option("client") ?
        ClientManager::instance().get_client(parser.option_value("client")).context()
      : context;

    if (parser.has_option("draft"))
    {
        Editor& editor = real_context.editor();
        DynamicSelectionList sels{editor.buffer(), editor.selections()};
        auto restore_sels = on_scope_end([&]{ editor.select(sels); real_context.change_editor(editor); });
        func(parser, real_context);
    }
    else
        func(parser, real_context);

    // force redraw of this client window
    if (parser.has_option("client") and real_context.has_window())
        real_context.window().forget_timestamp();
}

void exec_string(CommandParameters params, Context& context)
{
    context_wrap(params, context, [](const ParametersParser& parser, Context& context) {
        KeyList keys;
        for (auto& param : parser)
        {
            KeyList param_keys = parse_keys(param);
            keys.insert(keys.end(), param_keys.begin(), param_keys.end());
        }
        exec_keys(keys, context);
    });
}

void eval_string(CommandParameters params, Context& context)
{
    context_wrap(params, context, [](const ParametersParser& parser, Context& context) {
        String command;
        for (auto& param : parser)
            command += param + " ";
        CommandManager::instance().execute(command, context);
    });
}

void menu(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "auto-single", false },
                                      { "select-cmds", false } });

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

static String assist(String message, CharCount maxWidth)
{
    static const std::vector<String> assistant =
        { " ╭──╮   ",
          " │  │   ",
          " @  @  ╭",
          " ││ ││ │",
          " ││ ││ ╯",
          " │╰─╯│  ",
          " ╰───╯  ",
          "        " };

    const CharCount maxBubbleWidth = maxWidth - assistant[0].char_length() - 6;
    CharCount bubbleWidth = 0;
    std::vector<String> lines;
    {
        using Utf8It = utf8::utf8_iterator<String::iterator>;
        Utf8It word_begin{message.begin()};
        Utf8It word_end{word_begin};
        Utf8It end{message.end()};
        CharCount col = 0;
        String line;
        while (word_begin != end)
        {
            do
            {
                ++word_end;
            } while (word_end != end and not is_blank(*word_end) and not is_eol(*word_end));

            col += word_end - word_begin;
            if (col > maxBubbleWidth or *word_begin == '\n')
            {
                bubbleWidth = std::max(bubbleWidth, line.char_length());
                lines.push_back(std::move(line));
                line = "";
                col = 0;
            }
            if (*word_begin != '\n')
                line += String{word_begin.base(), word_end.base()};
            word_begin = word_end;
        }
        if (not line.empty())
        {
            bubbleWidth = std::max(bubbleWidth, line.char_length());
            lines.push_back(std::move(line));
        }
    }

    String result;
    LineCount lineCount{std::max<int>(assistant.size()-1, lines.size() + 2)};
    for (LineCount i = 0; i < lineCount; ++i)
    {

        result += assistant[std::min((int)i, (int)assistant.size()-1)];
        if (i == 0)
            result += "╭─" + String(Codepoint{L'─'}, bubbleWidth) + "─╮";
        else if (i < lines.size() + 1)
        {
            auto& line = lines[(int)i - 1];
            const CharCount padding = std::max(bubbleWidth - line.char_length(), 0_char);
            result += "│ " + line + String(' ', padding) + " │";
        }
        else if (i == lines.size() + 1)
            result += "╰─" + String(Codepoint{L'─'}, bubbleWidth) + "─╯";

        result += "\n";
    }
    return result;
}


void info(CommandParameters params, Context& context)
{
    ParametersParser parser(params, { { "anchor", true }, { "assist", false } },
                            ParametersParser::Flags::None, 0, 1);

    context.ui().info_hide();
    if (parser.positional_count() > 0)
    {
        MenuStyle style = MenuStyle::Prompt;
        DisplayCoord dimensions = context.ui().dimensions();
        DisplayCoord pos = { dimensions.line, 0 };
        if (parser.has_option("anchor"))
        {
            style =  MenuStyle::Inline;
            const auto& sel = context.editor().main_selection();
            auto it = sel.last();
            String anchor = parser.option_value("anchor");
            if (anchor == "left")
                it = sel.min();
            else if (anchor == "right")
                it = sel.max();
            else if (anchor != "cursor")
                throw runtime_error("anchor param must be one of [left, right, cursor]");
            pos = context.window().display_position(it);
        }
        const String& message = parser.has_option("assist") ? assist(parser[0], dimensions.column) : parser[0];
        context.ui().info_show(message, pos, get_color("Information"), style);
    }
}

void try_catch(CommandParameters params, Context& context)
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

void define_color_alias(CommandParameters params, Context& context)
{
    ParametersParser parser(params, OptionMap{},
                            ParametersParser::Flags::None, 2, 2);
    ColorRegistry::instance().register_alias(
        parser[0], parser[1], true);
}

void set_client_name(CommandParameters params, Context& context)
{
    ParametersParser parser(params, OptionMap{},
                            ParametersParser::Flags::None, 1, 1);
    auto& manager = ClientManager::instance();
    manager.set_client_name(manager.get_client(context), params[0]);
}

void set_register(CommandParameters params, Context& context)
{
    if (params.size() != 2)
        throw wrong_argument_count();

    if (params[0].length() != 1)
        throw runtime_error("register names are single character");
    RegisterManager::instance()[params[0][0]] = memoryview<String>(params[1]);
}

void change_working_directory(CommandParameters params, Context&)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    if (chdir(parse_filename(params[0]).c_str()) != 0)
        throw runtime_error("cannot change to directory " + params[0]);
}

template<typename GetRootGroup>
CommandCompleter group_rm_completer(GetRootGroup get_root_group)
{
    return [=](const Context& context, CommandParameters params,
               size_t token_to_complete, ByteCount pos_in_token) {
        auto& root_group = get_root_group(context);
        const String& arg = token_to_complete < params.size() ?
                            params[token_to_complete] : String();
        if (token_to_complete == 1 and params[0] == "-group")
            return root_group.complete_group_id(arg, pos_in_token);
        else if (token_to_complete == 2 and params[0] == "-group")
            return get_group(root_group, params[1]).complete_id(arg, pos_in_token);
        return root_group.complete_id(arg, pos_in_token);
    };
}

template<typename FactoryRegistry, typename GetRootGroup>
CommandCompleter group_add_completer(GetRootGroup get_root_group)
{
    return [=](const Context& context, CommandParameters params,
               size_t token_to_complete, ByteCount pos_in_token) {
        auto& root_group = get_root_group(context);
        const String& arg = token_to_complete < params.size() ?
                            params[token_to_complete] : String();
        if (token_to_complete == 1 and params[0] == "-group")
            return root_group.complete_group_id(arg, pos_in_token);
        else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
            return FactoryRegistry::instance().complete_name(arg, pos_in_token);
        return CandidateList();
    };
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

class BatchUI : public UserInterface
{
public:
    BatchUI(const KeyList& keys)
      : m_keys(keys), m_pos(0) {}

    Key get_key() override
    {
        kak_assert(m_pos < m_keys.size());
        return m_keys[m_pos++];
    }
    bool is_key_available() override { return m_pos < m_keys.size(); }

    void print_status(const DisplayLine&) override {}
    void draw(const DisplayBuffer&, const DisplayLine&) override {}
    void menu_show(memoryview<String>,
                   DisplayCoord, ColorPair, ColorPair, MenuStyle) override {}
    void menu_select(int) override {}
    void menu_hide() override {}

    void info_show(const String&, DisplayCoord, ColorPair, MenuStyle) override {}
    void info_hide() override {}

    DisplayCoord dimensions() override { return { 0, 0 }; }

    void set_input_callback(InputCallback callback) {}

private:
    const KeyList& m_keys;
    size_t         m_pos;
};

}

void exec_keys(const KeyList& keys, Context& context)
{
    RegisterRestorer quote('"', context);
    RegisterRestorer slash('/', context);

    scoped_edition edition(context.editor());

    BatchUI batch_ui(keys);
    InputHandler batch_input_handler(batch_ui);
    batch_input_handler.context().change_editor(context.editor());

    batch_input_handler.handle_available_inputs();

    auto& new_editor = batch_input_handler.context().editor();
    if (&new_editor != &context.editor())
    {
        context.push_jump();
        context.change_editor(new_editor);
    }
}


void register_commands()
{
    CommandManager& cm = CommandManager::instance();

    cm.register_commands({"nop"}, [](CommandParameters, Context&){});

    PerArgumentCommandCompleter filename_completer({
         [](const Context& context, const String& prefix, ByteCount cursor_pos)
         { return complete_filename(prefix, context.options()["ignored_files"].get<Regex>(), cursor_pos); }
    });
    cm.register_commands({ "e", "edit" }, edit<false>, filename_completer);
    cm.register_commands({ "e!", "edit!" }, edit<true>, filename_completer);
    cm.register_commands({ "w", "write" }, write_buffer, filename_completer);
    cm.register_commands({ "wa", "writeall" }, write_all_buffers);
    cm.register_commands({ "q", "quit" }, quit<false>);
    cm.register_commands({ "q!", "quit!" }, quit<true>);
    cm.register_command("wq", write_and_quit<false>);
    cm.register_command("wq!", write_and_quit<true>);

    PerArgumentCommandCompleter buffer_completer({
        [](const Context& context, const String& prefix, ByteCount cursor_pos)
        { return BufferManager::instance().complete_buffername(prefix, cursor_pos); }
    });
    cm.register_commands({ "b", "buffer" }, show_buffer, buffer_completer);
    cm.register_commands({ "db", "delbuf" }, delete_buffer<false>, buffer_completer);
    cm.register_commands({ "db!", "delbuf!" }, delete_buffer<true>, buffer_completer);
    cm.register_commands({"nb", "namebuf"}, set_buffer_name);

    auto get_highlighters = [](const Context& c) -> HighlighterGroup& { return c.window().highlighters(); };
    auto get_filters      = [](const Context& c) -> FilterGroup& { return c.window().filters(); };
    cm.register_commands({ "ah", "addhl" }, add_highlighter, group_add_completer<HighlighterRegistry>(get_highlighters));
    cm.register_commands({ "rh", "rmhl" }, rm_highlighter, group_rm_completer(get_highlighters));
    cm.register_commands({ "af", "addfilter" }, add_filter, group_add_completer<FilterRegistry>(get_filters));
    cm.register_commands({ "rf", "rmfilter" }, rm_filter, group_rm_completer(get_filters));

    cm.register_command("hook", add_hook);
    cm.register_command("rmhooks", rm_hooks);

    cm.register_command("source", exec_commands_in_file, filename_completer);

    cm.register_command("exec", exec_string);
    cm.register_command("eval", eval_string);
    cm.register_command("menu", menu);
    cm.register_command("info", info);
    cm.register_command("try",  try_catch);
    cm.register_command("reg", set_register);

    cm.register_command("def",  define_command);
    cm.register_command("decl", declare_option);

    cm.register_command("echo", echo_message);
    cm.register_command("debug", write_debug_message);

    cm.register_commands({ "setg", "setglobal" }, set_global_option,
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, ByteCount cursor_pos)
                             { return GlobalOptions::instance().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setb", "setbuffer" }, set_buffer_option,
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, ByteCount cursor_pos)
                             { return context.buffer().options().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setw", "setwindow" }, set_window_option,
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, ByteCount cursor_pos)
                             { return context.window().options().complete_option_name(prefix, cursor_pos); }
                         }));

    cm.register_commands({"ca", "colalias"}, define_color_alias);
    cm.register_commands({"nc", "nameclient"}, set_client_name);

    cm.register_command("cd", change_working_directory, filename_completer);
}
}
