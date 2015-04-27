#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "containers.hh"
#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "highlighters.hh"
#include "insert_completer.hh"
#include "shared_string.hh"
#include "ncurses_ui.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "scope.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "window.hh"

#include <fcntl.h>
#include <locale>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace Kakoune;

void run_unit_tests();

String runtime_directory()
{
    String bin_path = get_kak_binary_path();
    auto it = find(reversed(bin_path), '/');
    if (it == bin_path.rend())
        throw runtime_error("unable to determine runtime directory");
    return StringView{bin_path.begin(), it.base()-1} + "/../share/kak";
}

static void write(int fd, StringView str)
{
    write(fd, str.data(), (size_t)(int)str.length());
}

static void write_stdout(StringView str) { write(1, str); }
static void write_stderr(StringView str) { write(2, str); }

void register_env_vars()
{
    static const struct {
        const char* name;
        String (*func)(StringView, const Context&);
    } env_vars[] = { {
            "bufname",
            [](StringView name, const Context& context)
            { return context.buffer().display_name(); }
        }, {
            "buffile",
            [](StringView name, const Context& context) -> String
            { return context.buffer().name(); }
        }, {
            "buflist",
            [](StringView name, const Context& context)
            { return join(transformed(BufferManager::instance(),
                                      [](const SafePtr<Buffer>& b)
                                      { return b->display_name(); }), ':'); }
        }, {
            "timestamp",
            [](StringView name, const Context& context) -> String
            { return to_string(context.buffer().timestamp()); }
        }, {
            "selection",
            [](StringView name, const Context& context)
            { const Selection& sel = context.selections().main();
              return content(context.buffer(), sel); }
        }, {
            "selections",
            [](StringView name, const Context& context)
            { return join(context.selections_content(), ':'); }
        }, {
            "runtime",
            [](StringView name, const Context& context)
            { return runtime_directory(); }
        }, {
            "opt_.+",
            [](StringView name, const Context& context)
            { return context.options()[name.substr(4_byte)].get_as_string(); }
        }, {
            "reg_.+",
            [](StringView name, const Context& context)
            { return context.main_sel_register_value(name.substr(4_byte)).str(); }
        }, {
            "client_env_.+",
            [](StringView name, const Context& context)
            { return context.client().get_env_var(name.substr(11_byte)).str(); }
        }, {
            "session",
            [](StringView name, const Context& context) -> String
            { return Server::instance().session(); }
        }, {
            "client",
            [](StringView name, const Context& context) -> String
            { return context.name(); }
        }, {
            "cursor_line",
            [](StringView name, const Context& context) -> String
            { return to_string(context.selections().main().cursor().line + 1); }
        }, {
            "cursor_column",
            [](StringView name, const Context& context) -> String
            { return to_string(context.selections().main().cursor().column + 1); }
        }, {
            "cursor_char_column",
            [](StringView name, const Context& context) -> String
            { auto coord = context.selections().main().cursor();
              return to_string(context.buffer()[coord.line].char_count_to(coord.column) + 1); }
        }, {
            "selection_desc",
            [](StringView name, const Context& context)
            { return selection_to_string(context.buffer(), context.selections().main()); }
        }, {
            "selections_desc",
            [](StringView name, const Context& context)
            { return selection_list_to_string(context.selections()); }
        }, {
            "window_width",
            [](StringView name, const Context& context) -> String
            { return to_string(context.window().dimensions().column); }
        }, {
            "window_height",
            [](StringView name, const Context& context) -> String
            { return to_string(context.window().dimensions().line); }
    } };

    ShellManager& shell_manager = ShellManager::instance();
    for (auto& env_var : env_vars)
        shell_manager.register_env_var(env_var.name, env_var.func);
}

void register_registers()
{
    using StringList = Vector<String, MemoryDomain::Registers>;
    static const struct {
        char name;
        StringList (*func)(const Context&);
    } dyn_regs[] = {
        { '%', [](const Context& context) { return StringList{{context.buffer().display_name()}}; } },
        { '.', [](const Context& context) {
            auto content = context.selections_content();
            return StringList{content.begin(), content.end()};
        } },
        { '#', [](const Context& context) {
            StringList res;
            for (size_t i = 1; i < context.selections().size()+1; ++i)
                res.push_back(to_string((int)i));
            return res;
        } }
    };

    RegisterManager& register_manager = RegisterManager::instance();
    for (auto& dyn_reg : dyn_regs)
        register_manager.register_dynamic_register(dyn_reg.name, dyn_reg.func);

    for (size_t i = 0; i < 10; ++i)
    {
        register_manager.register_dynamic_register('0'+i,
            [i](const Context& context) {
                StringList result;
                for (auto& sel : context.selections())
                    result.emplace_back(i < sel.captures().size() ? sel.captures()[i] : "");
                return result;
            });
    }
}

void register_options()
{
    OptionsRegistry& reg = GlobalScope::instance().option_registry();

    reg.declare_option("tabstop", "size of a tab character", 8);
    reg.declare_option("indentwidth", "indentation width", 4);
    reg.declare_option("scrolloff",
                       "number of lines and columns to keep visible main cursor when scrolling",
                       CharCoord{0,0});
    reg.declare_option("eolformat", "end of line format: 'crlf' or 'lf'", "lf"_str);
    reg.declare_option("BOM", "insert a byte order mark when writing buffer",
                       "no"_str);
    reg.declare_option("complete_prefix",
                       "complete up to common prefix in tab completion",
                       true);
    reg.declare_option("incsearch",
                       "incrementaly apply search/select/split regex",
                       true);
    reg.declare_option("autoinfo",
                       "automatically display contextual help",
                       1);
    reg.declare_option("autoshowcompl",
                       "automatically display possible completions for prompts",
                       true);
    reg.declare_option("aligntab",
                       "use tab characters when possible for alignement",
                       false);
    reg.declare_option("ignored_files",
                       "patterns to ignore when completing filenames",
                       Regex{R"(^(\..*|.*\.(o|so|a))$)"});
    reg.declare_option("disabled_hooks",
                      "patterns to disable hooks whose group is matched",
                      Regex{});
    reg.declare_option("filetype", "buffer filetype", ""_str);
    reg.declare_option("path", "path to consider when trying to find a file",
                   Vector<String, MemoryDomain::Options>({ "./", "/usr/include" }));
    reg.declare_option("completers", "insert mode completers to execute.",
                       InsertCompleterDescList({
                           InsertCompleterDesc{ InsertCompleterDesc::Filename },
                           InsertCompleterDesc{ InsertCompleterDesc::Word, "all"_str }
                       }), OptionFlags::None);
    reg.declare_option("autoreload",
                       "autoreload buffer when a filesystem modification is detected",
                       Ask);
    reg.declare_option("ui_options",
                       "colon separated list of <key>=<value> options that are"
                       "passed to and interpreted by the user interface",
                       UserInterface::Options{});
}

template<typename UI>
void create_local_client(StringView init_command)
{
    struct LocalUI : UI
    {
        ~LocalUI()
        {
            if (not ClientManager::instance().empty() and fork())
            {
                this->UI::~UI();
                write_stdout("detached from terminal\n");
                exit(0);
            }
        }
    };

    if (std::is_same<UI, NCursesUI>::value)
    {
        if (not isatty(1))
            throw runtime_error("stdout is not a tty");

        if (not isatty(0))
        {
            // move stdin to another fd, and restore tty as stdin
            int fd = dup(0);
            int tty = open("/dev/tty", O_RDONLY);
            dup2(tty, 0);
            close(tty);
            create_fifo_buffer("*stdin*", fd);
        }
    }

    static Client* client = ClientManager::instance().create_client(
        make_unique<LocalUI>(), get_env_vars(), init_command);
    signal(SIGHUP, [](int) {
        if (client)
            ClientManager::instance().remove_client(*client);
        client = nullptr;
    });
}

void signal_handler(int signal)
{
    NCursesUI::abort();
    const char* text = nullptr;
    switch (signal)
    {
        case SIGSEGV: text = "SIGSEGV"; break;
        case SIGFPE:  text = "SIGFPE";  break;
        case SIGQUIT: text = "SIGQUIT"; break;
        case SIGTERM: text = "SIGTERM"; break;
    }
    if (signal != SIGTERM)
        on_assert_failed(text);

    if (Server::has_instance())
        Server::instance().close_session();
    if (BufferManager::has_instance())
        BufferManager::instance().backup_modified_buffers();

    if (signal == SIGTERM)
        exit(-1);
    else
        abort();
}

int run_client(StringView session, StringView init_command)
{
    try
    {
        EventManager event_manager;
        RemoteClient client{session, make_unique<NCursesUI>(),
                            get_env_vars(), init_command};
        while (true)
            event_manager.handle_next_events(EventMode::Normal);
    }
    catch (peer_disconnected&)
    {
        write_stderr("disconnected from server\n");
        return -1;
    }
    catch (connection_failed& e)
    {
        write_stderr(e.what());
        return -1;
    }
    return 0;
}

struct DummyUI : UserInterface
{
public:
    void menu_show(ConstArrayView<String>, CharCoord, Face, Face, MenuStyle) override {}
    void menu_select(int) override {}
    void menu_hide() override {}

    void info_show(StringView, StringView, CharCoord, Face, InfoStyle) override {}
    void info_hide() override {}

    void draw(const DisplayBuffer&, const DisplayLine&, const DisplayLine&) override {}
    CharCoord dimensions() override { return {24,80}; }
    bool is_key_available() override { return false; }
    Key  get_key() override { return Key::Invalid; }
    void refresh() override {}
    void set_input_callback(InputCallback) override {}
    void set_ui_options(const Options&) override {}
};

int run_server(StringView session, StringView init_command,
               bool ignore_kakrc, bool daemon, bool dummy_ui,
               ConstArrayView<StringView> files)
{
    static bool terminate = false;
    if (daemon)
    {
        if (session.empty())
        {
            write_stderr("-d needs a session name to be specified with -s\n");
            return -1;
        }
        if (pid_t child = fork())
        {
            write_stderr(format("Kakoune forked to background, for session '{}'\n"
                                "send SIGTERM to process {} for closing the session\n",
                                session, child));
            exit(0);
        }
        signal(SIGTERM, [](int) { terminate = true; });
    }

    StringRegistry      string_registry;
    EventManager        event_manager;
    GlobalScope         global_scope;
    ShellManager        shell_manager;
    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    DefinedHighlighters defined_highlighters;
    FaceRegistry        face_registry;
    ClientManager       client_manager;

    run_unit_tests();

    register_options();
    register_env_vars();
    register_registers();
    register_commands();
    register_highlighters();

    write_debug("*** This is the debug buffer, where debug info will be written ***");

    Server server(session.empty() ? to_string(getpid()) : session.str());

    if (not ignore_kakrc) try
    {
        Context initialisation_context{Context::EmptyContextFlag{}};
        command_manager.execute("source " + runtime_directory() + "/kakrc",
                                initialisation_context);
    }
    catch (Kakoune::runtime_error& error)
    {
        write_debug("error while parsing kakrc:\n    "_str + error.what());
    }
    catch (Kakoune::client_removed&)
    {
        write_debug("error while parsing kakrc: asked to quit");
    }

    {
        Context empty_context{Context::EmptyContextFlag{}};
        global_scope.hooks().run_hook("KakBegin", "", empty_context);
    }

    if (not files.empty()) try
    {
        // create buffers in reverse order so that the first given buffer
        // is the most recently created one.
        for (auto& file : reversed(files))
        {
            if (not create_buffer_from_file(file))
                new Buffer(file.str(), Buffer::Flags::New | Buffer::Flags::File);
        }
    }
    catch (Kakoune::runtime_error& error)
    {
         write_debug("error while opening command line files: "_str + error.what());
    }
    else
        new Buffer("*scratch*", Buffer::Flags::None);

    if (not daemon)
    {
        if (dummy_ui)
            create_local_client<DummyUI>(init_command);
        else
            create_local_client<NCursesUI>(init_command);
    }

    while (not terminate and (not client_manager.empty() or daemon))
    {
        client_manager.redraw_clients();
        event_manager.handle_next_events(EventMode::Normal);
        client_manager.handle_pending_inputs();
        client_manager.clear_mode_trashes();
        buffer_manager.clear_buffer_trash();
        string_registry.purge_unused();
    }

    {
        Context empty_context{Context::EmptyContextFlag{}};
        global_scope.hooks().run_hook("KakEnd", "", empty_context);
    }

    return 0;
}

int run_filter(StringView keystr, ConstArrayView<StringView> files, bool quiet)
{
    StringRegistry  string_registry;
    GlobalScope     global_scope;
    ShellManager    shell_manager;
    BufferManager   buffer_manager;
    RegisterManager register_manager;

    register_options();
    register_env_vars();
    register_registers();

    try
    {
        auto keys = parse_keys(keystr);

        auto apply_keys_to_buffer = [&](Buffer& buffer)
        {
            try
            {
                InputHandler input_handler{
                    { buffer, Selection{{0,0}, buffer.back_coord()} },
                    Context::Flags::Transient
                };

                for (auto& key : keys)
                    input_handler.handle_key(key);
            }
            catch (Kakoune::runtime_error& err)
            {
                if (not quiet)
                    write_stderr(format("error while applying keys to buffer '{}': {}\n",
                                        buffer.display_name(), err.what()));
            }
        };

        for (auto& file : files)
        {
            Buffer* buffer = create_buffer_from_file(file);
            write_buffer_to_file(*buffer, file + ".kak-bak");
            apply_keys_to_buffer(*buffer);
            write_buffer_to_file(*buffer, file);
            buffer_manager.delete_buffer(*buffer);
        }
        if (not isatty(0))
        {
            Buffer* buffer = create_buffer_from_data(read_fd(0), "*stdin*",
                                                     Buffer::Flags::None);
            apply_keys_to_buffer(*buffer);
            write_buffer_to_fd(*buffer, 1);
            buffer_manager.delete_buffer(*buffer);
        }
    }
    catch (Kakoune::runtime_error& err)
    {
        write_stderr(format("error: {}\n", err.what()));
    }

    buffer_manager.clear_buffer_trash();
    return 0;
}

int run_pipe(StringView session)
{
    char buf[512];
    String command;
    while (ssize_t count = read(0, buf, 512))
    {
        if (count < 0)
        {
            write_stderr("error while reading stdin\n");
            return -1;
        }
        command += StringView{buf, buf + count};
    }
    try
    {
        send_command(session, command);
    }
    catch (connection_failed& e)
    {
        write_stderr(e.what());
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");

    signal(SIGSEGV, signal_handler);
    signal(SIGFPE,  signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);

    Vector<String> params;
    for (size_t i = 1; i < argc; ++i)
        params.push_back(argv[i]);

    const ParameterDesc param_desc{
        SwitchMap{ { "c", { true,  "connect to given session" } },
                   { "e", { true,  "execute argument on initialisation" } },
                   { "n", { false, "do not source kakrc files on startup" } },
                   { "s", { true,  "set session name" } },
                   { "d", { false, "run as a headless session (requires -s)" } },
                   { "p", { true,  "just send stdin as commands to the given session" } },
                   { "f", { true,  "act as a filter, executing given keys on given files" } },
                   { "q", { false, "in filter mode, be quiet about errors applying keys" } },
                   { "u", { false, "use a dummy user interface, for testing purposes" } } }
    };
    try
    {
        std::sort(keymap.begin(), keymap.end(),
                  [](const NormalCmdDesc& lhs, const NormalCmdDesc& rhs)
                  { return lhs.key < rhs.key; });

        ParametersParser parser(params, param_desc);

        if (auto session = parser.get_switch("p"))
        {
            for (auto opt : { "c", "n", "s", "d", "e" })
            {
                if (parser.get_switch(opt))
                {
                    write_stderr(format("error: -{} makes not sense with -p\n", opt));
                    return -1;
                }
            }
            return run_pipe(*session);
        }
        else if (auto keys = parser.get_switch("f"))
        {
            Vector<StringView> files;
            for (size_t i = 0; i < parser.positional_count(); ++i)
                files.emplace_back(parser[i]);

            return run_filter(*keys, files, (bool)parser.get_switch("q"));
        }

        auto init_command = parser.get_switch("e").value_or(StringView{});

        if (auto server_session = parser.get_switch("c"))
        {
            for (auto opt : { "n", "s", "d" })
            {
                if (parser.get_switch(opt))
                {
                    write_stderr(format("error: -{} makes not sense with -c\n", opt));
                    return -1;
                }
            }
            return run_client(*server_session, init_command);
        }
        else
        {
            Vector<StringView> files;
            for (size_t i = 0; i < parser.positional_count(); ++i)
                files.emplace_back(parser[i]);

            StringView session = parser.get_switch("s").value_or(StringView{});
            return run_server(session, init_command,
                              (bool)parser.get_switch("n"),
                              (bool)parser.get_switch("d"),
                              (bool)parser.get_switch("u"),
                              files);
        }
    }
    catch (Kakoune::parameter_error& error)
    {
        write_stderr(format("Error: {}\n"
                            "Valid switches:\n"
                            "{}", error.what(),
                            generate_switches_doc(param_desc.switches)));
       return -1;
    }
    catch (Kakoune::exception& error)
    {
        on_assert_failed(format("uncaught exception ({}):\n{}", typeid(error).name(), error.what()).c_str());
        return -1;
    }
    catch (std::exception& error)
    {
        on_assert_failed(format("uncaught exception ({}):\n{}", typeid(error).name(), error.what()).c_str());
        return -1;
    }
    catch (...)
    {
        on_assert_failed("uncaught exception");
        return -1;
    }
    return 0;
}
