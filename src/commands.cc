#include "commands.hh"

#include "command_manager.hh"
#include "buffer_manager.hh"
#include "option_manager.hh"
#include "context.hh"
#include "buffer.hh"
#include "window.hh"
#include "file.hh"
#include "input_handler.hh"
#include "string.hh"
#include "highlighter.hh"
#include "filter.hh"
#include "register_manager.hh"
#include "completion.hh"
#include "shell_manager.hh"
#include "event_manager.hh"
#include "color_registry.hh"
#include "client_manager.hh"
#include "parameters_parser.hh"
#include "utf8_iterator.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace Kakoune
{

namespace
{

struct wrong_argument_count : runtime_error
{
    wrong_argument_count() : runtime_error("wrong argument count") {}
};

Buffer* open_or_create(const String& filename, Context& context)
{
    Buffer* buffer = create_buffer_from_file(filename);
    if (not buffer)
    {
        context.print_status("new file " + filename);
        buffer = new Buffer(filename, Buffer::Flags::File | Buffer::Flags::New);
    }
    return buffer;
}

Buffer* open_fifo(const String& name , const String& filename, Context& context)
{
    int fd = open(filename.c_str(), O_RDONLY);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0)
       throw runtime_error("unable to open " + filename);
    Buffer* buffer = new Buffer(name, Buffer::Flags::Fifo | Buffer::Flags::NoUndo);

    auto watcher = new FDWatcher(fd, [buffer](FDWatcher& watcher) {
        constexpr size_t buffer_size = 1024 * 16;
        char data[buffer_size];
        ssize_t count = read(watcher.fd(), data, buffer_size);
        buffer->insert(buffer->end()-1,
                       count > 0 ? String(data, data+count)
                                  : "*** kak: fifo closed ***\n");
        ClientManager::instance().redraw_clients();
        if (count <= 0)
        {
            assert(buffer->flags() & Buffer::Flags::Fifo);
            buffer->flags() &= ~Buffer::Flags::Fifo;
            close(watcher.fd());
            delete &watcher;
        }
    });

    buffer->hooks().add_hook("BufClose",
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
            buffer = new Buffer(name, Buffer::Flags::None);
        else if (parser.has_option("fifo"))
            buffer = open_fifo(name, parser.option_value("fifo"), context);
        else
            buffer = open_or_create(name, context);
    }

    BufferManager::instance().set_last_used_buffer(*buffer);

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

        context.editor().select(context.buffer().iterator_at({ line,  column }));
        if (context.has_window())
            context.window().center_selection();
    }
}

void write_buffer(const CommandParameters& params, Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    Buffer& buffer = context.buffer();

    if (params.empty() and !(buffer.flags() & Buffer::Flags::File))
        throw runtime_error("cannot write a non file buffer without a filename");

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
        if ((buffer->flags() & Buffer::Flags::File) and buffer->is_modified())
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

    BufferManager::instance().set_last_used_buffer(*buffer);

    if (buffer != &context.buffer())
    {
        context.push_jump();
        auto& manager = ClientManager::instance();
        context.change_editor(manager.get_unused_window_for_buffer(*buffer));
    }
}

template<bool force>
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
    if (not force and (buffer->flags() & Buffer::Flags::File) and buffer->is_modified())
        throw runtime_error("buffer " + buffer->name() + " is modified");

    if (manager.count() == 1)
        throw runtime_error("buffer " + buffer->name() + " is the last one");

    ClientManager::instance().ensure_no_client_uses_buffer(*buffer);
    delete buffer;
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
        get_group(window.highlighters(), parser.option_value("group"))
      : window.highlighters();

    auto& factory = registry[name];
    group.append(factory(highlighter_params, window));
}

void rm_highlighter(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() != 1)
        throw wrong_argument_count();

    Window& window = context.window();
    HighlighterGroup& group = parser.has_option("group") ?
        get_group(window.highlighters(), parser.option_value("group"))
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

    Editor& editor = context.editor();
    FilterGroup& group = parser.has_option("group") ?
        get_group(editor.filters(), parser.option_value("group"))
      : editor.filters();

    auto& factory = registry[name];
    group.append(factory(filter_params));
}

void rm_filter(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "group", true } });
    if (parser.positional_count() != 1)
        throw wrong_argument_count();

    Editor& editor = context.editor();
    FilterGroup& group = parser.has_option("group") ?
        get_group(editor.filters(), parser.option_value("group"))
      : editor.filters();

    group.remove(parser[0]);
}

void add_hook(const CommandParameters& params, Context& context)
{
    if (params.size() != 4)
        throw wrong_argument_count();

    // copy so that the lambda gets a copy as well
    Regex regex(params[2].begin(), params[2].end());
    String command = params[3];
    auto hook_func = [=](const String& param, Context& context) {
        if (boost::regex_match(param.begin(), param.end(), regex))
            CommandManager::instance().execute(command, context);
    };

    const String& scope = params[0];
    const String& name = params[1];
    if (scope == "global")
        GlobalHooks::instance().add_hook(name, hook_func);
    else if (scope == "buffer")
        context.buffer().hooks().add_hook(name, hook_func);
    else if (scope == "window")
        context.window().hooks().add_hook(name , hook_func);
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
                              { "shell-params", false },
                              { "allow-override", false },
                              { "file-completion", false },
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
            CommandManager::instance().execute(commands, context, {},
                                               params_to_env_var_map(params));
        };
    }
    if (parser.has_option("shell-params"))
    {
        cmd = [=](const CommandParameters& params, Context& context) {
            CommandManager::instance().execute(commands, context, params, {});
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

    CommandCompleter completer;
    if (parser.has_option("file-completion"))
    {
        completer = [](const Context& context, const CommandParameters& params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = token_to_complete < params.size() ?
                                    params[token_to_complete] : String();
             return complete_filename(context, prefix, pos_in_token);
        };
    }
    else if (parser.has_option("shell-completion"))
    {
        String shell_cmd = parser.option_value("shell-completion");
        completer = [=](const Context& context, const CommandParameters& params,
                        size_t token_to_complete, ByteCount pos_in_token)
        {
           EnvVarMap vars = {
               {"token_to_complete", int_to_str(token_to_complete) },
               { "pos_in_token",     int_to_str((int)pos_in_token) }
           };
           String output = ShellManager::instance().eval(shell_cmd, context, params, vars);
           return split(output, '\n');
        };
    }
    CommandManager::instance().register_command(cmd_name, cmd, completer);
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

void set_option(OptionManager& options, const CommandParameters& params,
                Context& context)
{
    if (params.size() != 2)
        throw wrong_argument_count();

    options.get_local_option(params[0]).set_from_string(params[1]);
}

void declare_option(const CommandParameters& params, Context& context)
{
    if (params.size() != 2 and params.size() != 3)
        throw wrong_argument_count();
    Option* opt = nullptr;

    if (params[0] == "int")
        opt = &GlobalOptions::instance().declare_option<int>(params[1], 0);
    else if (params[0] == "str")
        opt = &GlobalOptions::instance().declare_option<String>(params[1], "");
    else if (params[0] == "int-list")
        opt = &GlobalOptions::instance().declare_option<std::vector<int>>(params[1], {});
    else
        throw runtime_error("unknown type " + params[0]);

    if (params.size() == 3)
        opt->set_from_string(params[2]);
}

template<typename Func>
void context_wrap(const CommandParameters& params, Context& context, Func func)
{
    ParametersParser parser(params, { { "client", true }, { "restore-selections", false }});
    if (parser.positional_count() == 0)
        throw wrong_argument_count();

    Context& real_context = parser.has_option("client") ?
        ClientManager::instance().get_client_context(parser.option_value("client"))
      : context;

    if (parser.has_option("restore-selections"))
    {
        Editor& editor = real_context.editor();
        DynamicSelectionList sels(editor.buffer(), editor.selections());
        auto restore_sels = on_scope_end([&]{ editor.select(sels); });
        func(parser, real_context);
    }
    else
        func(parser, real_context);
}

void exec_string(const CommandParameters& params, Context& context)
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

void eval_string(const CommandParameters& params, Context& context)
{
    context_wrap(params, context, [](const ParametersParser& parser, Context& context) {
        String command;
        for (auto& param : parser)
            command += param + " ";
        CommandManager::instance().execute(command, context);
    });
}

void menu(const CommandParameters& params, Context& context)
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
                line += String(word_begin.underlying_iterator(), word_end.underlying_iterator());
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
            result += "╭─" + String(Codepoint{L'─'}, (int)bubbleWidth) + "─╮";
        else if (i < lines.size() + 1)
        {
            auto& line = lines[(int)i - 1];
            const CharCount padding = std::max(bubbleWidth - line.char_length(), 0_char);
            result += "│ " + line + String(' ', padding) + " │";
        }
        else if (i == lines.size() + 1)
            result += "╰─" + String(Codepoint{L'─'}, (int)bubbleWidth) + "─╯";

        result += "\n";
    }
    return result;
}


void info(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, { { "anchor", true }, { "assist", false } });

    if (parser.positional_count() > 1)
        throw wrong_argument_count();

    context.ui().info_hide();
    if (parser.positional_count() > 0)
    {
        MenuStyle style = MenuStyle::Prompt;
        DisplayCoord dimensions = context.ui().dimensions();
        DisplayCoord pos = { dimensions.line, 0 };
        if (parser.has_option("anchor"))
        {
            style =  MenuStyle::Inline;
            const auto& sel = context.editor().selections().back();
            auto it = sel.last();
            String anchor = parser.option_value("anchor");
            if (anchor == "left")
                it = sel.begin();
            else if (anchor == "right")
                it = sel.end() - 1;
            else if (anchor != "cursor")
                throw runtime_error("anchor param must be one of [left, right, cursor]");
            pos = context.window().display_position(it);
        }
        const String& message = parser.has_option("assist") ? assist(parser[0], dimensions.column) : parser[0];
        context.ui().info_show(message, pos, style);
    }
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

void define_color_alias(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, {});
    if (parser.positional_count() != 2)
        throw wrong_argument_count();
    ColorRegistry::instance().register_alias(
        parser[0], parser[1], true);
}

void set_client_name(const CommandParameters& params, Context& context)
{
    ParametersParser parser(params, {});
    if (parser.positional_count() != 1)
        throw wrong_argument_count();
    ClientManager::instance().set_client_name(context, params[0]);
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
        assert(m_pos < m_keys.size());
        return m_keys[m_pos++];
    }
    bool is_key_available() override { return m_pos < m_keys.size(); }

    void print_status(const String& , CharCount) override {}
    void draw(const DisplayBuffer&, const String&) override {}
    void menu_show(const memoryview<String>&,
                   const DisplayCoord&, MenuStyle) override {}
    void menu_select(int) override {}
    void menu_hide() override {}

    void info_show(const String&, const DisplayCoord&, MenuStyle) override {}
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

    cm.register_commands({"nop"}, [](const CommandParameters&, Context&){});

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
        [](const Context& context, const String& prefix, ByteCount cursor_pos)
        { return BufferManager::instance().complete_buffername(prefix, cursor_pos); }
    });
    cm.register_commands({ "b", "buffer" }, show_buffer, buffer_completer);
    cm.register_commands({ "db", "delbuf" }, delete_buffer<false>, buffer_completer);
    cm.register_commands({ "db!", "delbuf!" }, delete_buffer<true>, buffer_completer);

    cm.register_commands({ "ah", "addhl" }, add_highlighter,
                         [](const Context& context, const CommandParameters& params,
                           size_t token_to_complete, ByteCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.highlighters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
                                 return HighlighterRegistry::instance().complete_name(arg, pos_in_token);
                             else
                                 return CandidateList();
                         });
    cm.register_commands({ "rh", "rmhl" }, rm_highlighter,
                         [](const Context& context, const CommandParameters& params,
                            size_t token_to_complete, ByteCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.highlighters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 2 and params[0] == "-group")
                                 return get_group(w.highlighters(), params[1]).complete_id(arg, pos_in_token);
                             else
                                 return w.highlighters().complete_id(arg, pos_in_token);
                         });
    cm.register_commands({ "af", "addfilter" }, add_filter,
                         [](const Context& context, const CommandParameters& params,
                            size_t token_to_complete, ByteCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.filters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
                                 return FilterRegistry::instance().complete_name(arg, pos_in_token);
                             else
                                 return CandidateList();
                         });
    cm.register_commands({ "rf", "rmfilter" }, rm_filter,
                         [](const Context& context, const CommandParameters& params,
                            size_t token_to_complete, ByteCount pos_in_token)
                         {
                             Window& w = context.window();
                             const String& arg = token_to_complete < params.size() ?
                                                 params[token_to_complete] : String();
                             if (token_to_complete == 1 and params[0] == "-group")
                                 return w.filters().complete_group_id(arg, pos_in_token);
                             else if (token_to_complete == 2 and params[0] == "-group")
                                 return get_group(w.filters(), params[1]).complete_id(arg, pos_in_token);
                             else
                                 return w.filters().complete_id(arg, pos_in_token);
                         });

    cm.register_command("hook", add_hook);

    cm.register_command("source", exec_commands_in_file, filename_completer);

    cm.register_command("exec", exec_string);
    cm.register_command("eval", eval_string);
    cm.register_command("menu", menu);
    cm.register_command("info", info);
    cm.register_command("try",  try_catch);

    cm.register_command("def",  define_command);
    cm.register_command("echo", echo_message);

    cm.register_commands({ "setg", "setglobal" },
                         [](const CommandParameters& params, Context& context)
                         { set_option(GlobalOptions::instance(), params, context); },
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, ByteCount cursor_pos)
                             { return GlobalOptions::instance().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setb", "setbuffer" },
                         [](const CommandParameters& params, Context& context)
                         { set_option(context.buffer().options(), params, context); },
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, ByteCount cursor_pos)
                             { return context.buffer().options().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_commands({ "setw", "setwindow" },
                         [](const CommandParameters& params, Context& context)
                         { set_option(context.window().options(), params, context); },
                         PerArgumentCommandCompleter({
                             [](const Context& context, const String& prefix, ByteCount cursor_pos)
                             { return context.window().options().complete_option_name(prefix, cursor_pos); }
                         }));
    cm.register_command("decl", declare_option);

    cm.register_commands({"ca", "colalias"}, define_color_alias);
    cm.register_commands({"name"}, set_client_name);
}

}
