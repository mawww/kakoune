#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "containers.hh"
#include "context.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "highlighters.hh"
#include "insert_completer.hh"
#include "shared_string.hh"
#include "ncurses_ui.hh"
#include "json_ui.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "scope.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "unit_tests.hh"
#include "window.hh"

#include <fcntl.h>
#include <locale>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

using namespace Kakoune;

struct startup_error : Kakoune::runtime_error
{
    using Kakoune::runtime_error::runtime_error;
};

String runtime_directory()
{
    char relpath[PATH_MAX+1];
    format_to(relpath, "{}../share/kak", split_path(get_kak_binary_path()).first);
    struct stat st;
    if (stat(relpath, &st) == 0 and S_ISDIR(st.st_mode))
        return real_path(relpath);

    return "/usr/share/kak";
}

void register_env_vars()
{
    static const struct {
        const char* name;
        bool prefix;
        String (*func)(StringView, const Context&);
    } env_vars[] = { {
            "bufname", false,
            [](StringView name, const Context& context) -> String
            { return context.buffer().display_name(); }
        }, {
            "buffile", false,
            [](StringView name, const Context& context) -> String
            { return context.buffer().name(); }
        }, {
            "buflist", false,
            [](StringView name, const Context& context)
            { return join(BufferManager::instance() |
                          transform([](const std::unique_ptr<Buffer>& b)
                                    { return b->display_name(); }), ':'); }
        }, {
            "timestamp", false,
            [](StringView name, const Context& context) -> String
            { return to_string(context.buffer().timestamp()); }
        }, {
            "selection", false,
            [](StringView name, const Context& context)
            { const Selection& sel = context.selections().main();
              return content(context.buffer(), sel); }
        }, {
            "selections", false,
            [](StringView name, const Context& context)
            { return join(context.selections_content(), ':'); }
        }, {
            "runtime", false,
            [](StringView name, const Context& context)
            { return runtime_directory(); }
        }, {
            "opt_", true,
            [](StringView name, const Context& context)
            { return context.options()[name.substr(4_byte)].get_as_string(); }
        }, {
            "reg_", true,
            [](StringView name, const Context& context)
            { return context.main_sel_register_value(name.substr(4_byte)).str(); }
        }, {
            "client_env_", true,
            [](StringView name, const Context& context)
            { return context.client().get_env_var(name.substr(11_byte)).str(); }
        }, {
            "session", false,
            [](StringView name, const Context& context) -> String
            { return Server::instance().session(); }
        }, {
            "client", false,
            [](StringView name, const Context& context) -> String
            { return context.name(); }
        }, {
            "cursor_line", false,
            [](StringView name, const Context& context) -> String
            { return to_string(context.selections().main().cursor().line + 1); }
        }, {
            "cursor_column", false,
            [](StringView name, const Context& context) -> String
            { return to_string(context.selections().main().cursor().column + 1); }
        }, {
            "cursor_char_column", false,
            [](StringView name, const Context& context) -> String
            { auto coord = context.selections().main().cursor();
              return to_string(context.buffer()[coord.line].char_count_to(coord.column) + 1); }
        }, {
            "cursor_byte_offset", false,
            [](StringView name, const Context& context) -> String
            { auto cursor = context.selections().main().cursor();
              return to_string(context.buffer().distance({0,0}, cursor)); }
        }, {
            "selection_desc", false,
            [](StringView name, const Context& context)
            { return selection_to_string(context.selections().main()); }
        }, {
            "selections_desc", false,
            [](StringView name, const Context& context)
            { return selection_list_to_string(context.selections()); }
        }, {
            "window_width", false,
            [](StringView name, const Context& context) -> String
            { return to_string(context.window().dimensions().column); }
        }, {
            "window_height", false,
            [](StringView name, const Context& context) -> String
            { return to_string(context.window().dimensions().line); }
    } };

    ShellManager& shell_manager = ShellManager::instance();
    for (auto& env_var : env_vars)
        shell_manager.register_env_var(env_var.name, env_var.prefix, env_var.func);
}

void register_registers()
{
    RegisterManager& register_manager = RegisterManager::instance();

    for (auto c : "abcdefghijklmnopqrstuvwxyz/\"|^@")
        register_manager.add_register(c, make_unique<StaticRegister>());

    using StringList = Vector<String, MemoryDomain::Registers>;

    register_manager.add_register('%', make_dyn_reg(
        [](const Context& context)
        { return StringList{{context.buffer().display_name()}}; }));

    register_manager.add_register('.', make_dyn_reg(
        [](const Context& context) {
            auto content = context.selections_content();
            return StringList{content.begin(), content.end()};
         }));

    register_manager.add_register('#', make_dyn_reg(
        [](const Context& context) {
            StringList res;
            for (size_t i = 1; i < context.selections().size()+1; ++i)
                res.push_back(to_string((int)i));
            return res;
        }));

    for (size_t i = 0; i < 10; ++i)
    {
        register_manager.add_register('0'+i, make_dyn_reg(
            [i](const Context& context) {
                StringList result;
                for (auto& sel : context.selections())
                    result.emplace_back(i < sel.captures().size() ? sel.captures()[i] : "");
                return result;
            }));
    }

    register_manager.add_register('_', make_unique<NullRegister>());
}

static void check_tabstop(const int& val)
{
    if (val < 1) throw runtime_error{"tabstop should be strictly positive"};
}

static void check_indentwidth(const int& val)
{
    if (val < 0) throw runtime_error{"indentwidth should be positive or zero"};
}

static void check_scrolloff(const CharCoord& so)
{
    if (so.line < 0 or so.column < 0)
        throw runtime_error{"scroll offset must be positive or zero"};
}

void register_options()
{
    OptionsRegistry& reg = GlobalScope::instance().option_registry();

    reg.declare_option<int, check_tabstop>("tabstop", "size of a tab character", 8);
    reg.declare_option<int, check_indentwidth>("indentwidth", "indentation width", 4);
    reg.declare_option<CharCoord, check_scrolloff>(
        "scrolloff", "number of lines and columns to keep visible main cursor when scrolling",
        {0,0});
    reg.declare_option("eolformat", "end of line format", EolFormat::Lf);
    reg.declare_option("BOM", "byte order mark to use when writing buffer",
                       ByteOrderMark::None);
    reg.declare_option("incsearch",
                       "incrementaly apply search/select/split regex",
                       true);
    reg.declare_option("autoinfo",
                       "automatically display contextual help",
                       AutoInfo::Command | AutoInfo::OnKey);
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
    reg.declare_option("static_words", "list of words to always consider for insert word completion",
                   Vector<String, MemoryDomain::Options>{});
    reg.declare_option("autoreload",
                       "autoreload buffer when a filesystem modification is detected",
                       Autoreload::Ask);
    reg.declare_option("ui_options",
                       "colon separated list of <key>=<value> options that are "
                       "passed to and interpreted by the user interface\n"
                       "\n"
                       "The ncurses ui supports the following options:\n"
                       "<key>:                        <value>:\n"
                       "    ncurses_assistant             clippy|cat|none|off\n"
                       "    ncurses_status_on_top         bool\n"
                       "    ncurses_set_title             bool\n"
                       "    ncurses_enable_mouse          bool\n"
                       "    ncurses_wheel_up_button       int\n"
                       "    ncurses_wheel_down_button     int\n"
                       "    ncurses_buffer_padding_str    str\n"
                       "    ncurses_buffer_padding_type   fill|single|off\n",
                       UserInterface::Options{});
    reg.declare_option("modelinefmt", "format string used to generate the modeline",
                       "%val{bufname} %val{cursor_line}:%val{cursor_char_column} "_str);
    reg.declare_option("debug", "various debug flags", DebugFlags::None);
    reg.declare_option("readonly", "prevent buffers from being modified", false);
    reg.declare_option("completion_extra_word_char",
                       "Additional characters to be considered as words for insert completion",
                       ""_str);
}

struct convert_to_client_mode
{
    String session;
    String buffer_name;
    String selections;
};

enum class UIType
{
    NCurses,
    Json,
    Dummy,
};

static Client* local_client = nullptr;
static UserInterface* local_ui = nullptr;
static bool convert_to_client_pending = false;

pid_t fork_server_to_background()
{
    if (pid_t pid = fork())
        return pid;

    if (fork()) // double fork to orphan the server
        exit(0);

    write_stderr(format("Kakoune forked server to background ({}), for session '{}'\n",
                        getpid(), Server::instance().session()));
    return 0;
}

std::unique_ptr<UserInterface> make_ui(UIType ui_type)
{
    struct DummyUI : UserInterface
    {
        DummyUI() { set_signal_handler(SIGINT, SIG_DFL); }
        void menu_show(ConstArrayView<DisplayLine>, CharCoord,
                       Face, Face, MenuStyle) override {}
        void menu_select(int) override {}
        void menu_hide() override {}

        void info_show(StringView, StringView, CharCoord, Face, InfoStyle) override {}
        void info_hide() override {}

        void draw(const DisplayBuffer&, const Face&, const Face&) override {}
        void draw_status(const DisplayLine&, const DisplayLine&, const Face&) override {}
        CharCoord dimensions() override { return {24,80}; }
        bool is_key_available() override { return false; }
        Key  get_key() override { return Key::Invalid; }
        void refresh(bool) override {}
        void set_input_callback(InputCallback) override {}
        void set_ui_options(const Options&) override {}
    };

    switch (ui_type)
    {
        case UIType::NCurses: return make_unique<NCursesUI>();
        case UIType::Json: return make_unique<JsonUI>();
        case UIType::Dummy: return make_unique<DummyUI>();
    }
    throw logic_error{};
}

std::unique_ptr<UserInterface> create_local_ui(UIType ui_type)
{
    if (ui_type != UIType::NCurses)
        return make_ui(ui_type);

    struct LocalUI : NCursesUI
    {
        LocalUI()
        {
            kak_assert(not local_ui);
            local_ui = this;
            m_old_sighup = set_signal_handler(SIGHUP, [](int) {
                ClientManager::instance().remove_client(*local_client, false);
            });

            m_old_sigtstp = set_signal_handler(SIGTSTP, [](int) {
                if (ClientManager::instance().count() == 1 and
                    *ClientManager::instance().begin() == local_client)
                {
                    // Suspend normally if we are the only client
                    auto current = set_signal_handler(SIGTSTP, static_cast<LocalUI*>(local_ui)->m_old_sigtstp);

                    sigset_t unblock_sigtstp, old_mask;
                    sigemptyset(&unblock_sigtstp);
                    sigaddset(&unblock_sigtstp, SIGTSTP);
                    sigprocmask(SIG_UNBLOCK, &unblock_sigtstp, &old_mask);

                    raise(SIGTSTP);

                    sigprocmask(SIG_SETMASK, &old_mask, nullptr);

                    set_signal_handler(SIGTSTP, current);
                }
                else
                    convert_to_client_pending = true;
           });
        }

        ~LocalUI()
        {
            set_signal_handler(SIGHUP, m_old_sighup);
            set_signal_handler(SIGTSTP, m_old_sigtstp);
            local_client = nullptr;
            local_ui = nullptr;
            if (not convert_to_client_pending and
                not ClientManager::instance().empty())
            {
                if (fork_server_to_background())
                {
                    this->NCursesUI::~NCursesUI();
                    exit(0);
                }
            }
        }

    private:
        using SigHandler = void (*)(int);
        SigHandler m_old_sighup;
        SigHandler m_old_sigtstp;
    };

    if (not isatty(1))
        throw startup_error("stdout is not a tty");

    if (not isatty(0))
    {
        // move stdin to another fd, and restore tty as stdin
        int fd = dup(0);
        int tty = open("/dev/tty", O_RDONLY);
        dup2(tty, 0);
        close(tty);
        create_fifo_buffer("*stdin*", fd);
    }

    return make_unique<LocalUI>();
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
        case SIGPIPE: text = "SIGPIPE"; break;
    }
    if (signal != SIGTERM)
    {
        auto msg = format("Received {}, exiting.\nPid: {}\nCallstack:\n{}",
                          text, getpid(), Backtrace{}.desc());
        write_stderr(msg);
        notify_fatal_error(msg);
    }

    if (Server::has_instance())
        Server::instance().close_session();
    if (BufferManager::has_instance())
        BufferManager::instance().backup_modified_buffers();

    if (signal == SIGTERM)
        exit(-1);
    else
        abort();
}

int run_client(StringView session, StringView init_command, UIType ui_type)
{
    try
    {
        EventManager event_manager;
        RemoteClient client{session, make_ui(ui_type), get_env_vars(), init_command};
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
        write_stderr(format("{}\n", e.what()));
        return -1;
    }
    return 0;
}

int run_server(StringView session, StringView init_command,
               bool ignore_kakrc, bool daemon, bool readonly, UIType ui_type,
               ConstArrayView<StringView> files, ByteCoord target_coord)
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
        set_signal_handler(SIGTERM, [](int) { terminate = true; });
    }

    StringRegistry      string_registry;
    EventManager        event_manager;
    GlobalScope         global_scope;
    ShellManager        shell_manager;
    CommandManager      command_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    DefinedHighlighters defined_highlighters;
    FaceRegistry        face_registry;
    ClientManager       client_manager;
    BufferManager       buffer_manager;

    register_options();
    register_env_vars();
    register_registers();
    register_commands();
    register_highlighters();

    UnitTest::run_all_tests();

    write_to_debug_buffer("*** This is the debug buffer, where debug info will be written ***");

    GlobalScope::instance().options().get_local_option("readonly").set(readonly);

    Server server(session.empty() ? to_string(getpid()) : session.str());

    bool startup_error = false;
    if (not ignore_kakrc) try
    {
        Context initialisation_context{Context::EmptyContextFlag{}};
        command_manager.execute(format("source {}/kakrc", runtime_directory()),
                                initialisation_context);
    }
    catch (Kakoune::runtime_error& error)
    {
        startup_error = true;
        write_to_debug_buffer(format("error while parsing kakrc:\n    {}", error.what()));
    }
    catch (Kakoune::client_removed&)
    {
        startup_error = true;
        write_to_debug_buffer("error while parsing kakrc: asked to quit");
    }

    {
        Context empty_context{Context::EmptyContextFlag{}};
        global_scope.hooks().run_hook("KakBegin", "", empty_context);
    }

    if (not files.empty()) try
    {
        // create buffers in reverse order so that the first given buffer
        // is the most recently created one.
        for (auto& file : files | reverse())
        {
            try
            {
                Buffer *buffer = open_or_create_file_buffer(file);
                if (readonly)
                    buffer->flags() |= Buffer::Flags::ReadOnly;
            }
            catch (Kakoune::runtime_error& error)
            {
                startup_error = true;
                write_to_debug_buffer(format("error while opening file '{}':\n    {}",
                                             file, error.what()));
            }
        }
    }
    catch (Kakoune::runtime_error& error)
    {
         write_to_debug_buffer(format("error while opening command line files: {}", error.what()));
    }
    else
        buffer_manager.create_buffer("*scratch*", Buffer::Flags::None);

    if (not daemon)
    {
        local_client = client_manager.create_client(
            create_local_ui(ui_type), get_env_vars(), init_command);

        if (local_client)
        {
            auto& selections = local_client->context().selections_write_only();
            auto& buffer = selections.buffer();
            selections = SelectionList(buffer, buffer.clamp(target_coord));
            local_client->context().window().center_line(target_coord.line);

            if (startup_error)
                local_client->print_status({
                    "error during startup, see *debug* buffer for details",
                    get_face("Error")
                });
        }
    }

    try
    {
        while (not terminate and (not client_manager.empty() or daemon))
        {
            client_manager.redraw_clients();
            event_manager.handle_next_events(EventMode::Normal);
            client_manager.handle_pending_inputs();
            client_manager.clear_window_trash();
            buffer_manager.clear_buffer_trash();
            string_registry.purge_unused();

            if (convert_to_client_pending)
            {
                String buffer_name = local_client->context().buffer().name();
                String selections = selection_list_to_string(local_client->context().selections());

                ClientManager::instance().remove_client(*local_client, true);
                convert_to_client_pending = false;

                if (fork_server_to_background())
                {
                    String session = server.session();
                    server.close_session(false);
                    throw convert_to_client_mode{ std::move(session), std::move(buffer_name), std::move(selections) };
                }
            }
        }
    }
    catch (const kill_session&) {}

    {
        Context empty_context{Context::EmptyContextFlag{}};
        global_scope.hooks().run_hook("KakEnd", "", empty_context);
    }

    return 0;
}

int run_filter(StringView keystr, StringView commands, ConstArrayView<StringView> files, bool quiet)
{
    StringRegistry  string_registry;
    GlobalScope     global_scope;
    EventManager    event_manager;
    ShellManager    shell_manager;
    CommandManager  command_manager;
    RegisterManager register_manager;
    ClientManager   client_manager;
    BufferManager   buffer_manager;

    register_options();
    register_env_vars();
    register_registers();
    register_commands();

    try
    {
        auto keys = parse_keys(keystr);

        auto apply_to_buffer = [&](Buffer& buffer)
        {
            try
            {
                InputHandler input_handler{
                    { buffer, Selection{{0,0}, buffer.back_coord()} },
                    Context::Flags::Transient
                };

                if (not commands.empty())
                    command_manager.execute(commands, input_handler.context(),
                                            ShellContext{});

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
            Buffer* buffer = open_file_buffer(file);
            write_buffer_to_file(*buffer, file + ".kak-bak");
            apply_to_buffer(*buffer);
            write_buffer_to_file(*buffer, file);
            buffer_manager.delete_buffer(*buffer);
        }
        if (not isatty(0))
        {
            Buffer& buffer = *buffer_manager.create_buffer(
                "*stdin*", Buffer::Flags::None, read_fd(0), InvalidTime);
            apply_to_buffer(buffer);
            write_buffer_to_fd(buffer, 1);
            buffer_manager.delete_buffer(buffer);
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
        write_stderr(format("{}\n", e.what()));
        return -1;
    }
    return 0;
}

UIType parse_ui_type(StringView ui_name)
{
    if (ui_name == "ncurses") return UIType::NCurses;
    if (ui_name == "json") return UIType::Json;
    if (ui_name == "dummy") return UIType::Dummy;

    throw parameter_error(format("error: unknown ui type: '{}'", ui_name));
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "");

    set_signal_handler(SIGSEGV, signal_handler);
    set_signal_handler(SIGFPE,  signal_handler);
    set_signal_handler(SIGQUIT, signal_handler);
    set_signal_handler(SIGTERM, signal_handler);
    set_signal_handler(SIGPIPE, SIG_IGN);
    set_signal_handler(SIGINT, [](int){});
    set_signal_handler(SIGCHLD, [](int){});

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
                   { "ui", { true, "set the type of user interface to use (ncurses, dummy, or json)" } },
                   { "l", { false, "list existing sessions" } },
                   { "clear", { false, "clear dead sessions" } },
                   { "ro", { false, "readonly mode" } } }
    };
    try
    {
        std::sort(keymap.begin(), keymap.end(),
                  [](const NormalCmdDesc& lhs, const NormalCmdDesc& rhs)
                  { return lhs.key < rhs.key; });

        ParametersParser parser(params, param_desc);

        const bool list_sessions = (bool)parser.get_switch("l");
        const bool clear_sessions = (bool)parser.get_switch("clear");
        if (list_sessions or clear_sessions)
        {
            StringView username = getpwuid(geteuid())->pw_name;
            for (auto& session : list_files(format("/tmp/kakoune/{}/", username)))
            {
                const bool valid = check_session(session);
                if (list_sessions)
                    write_stdout(format("{}{}\n", session, valid ? "" : " (dead)"));
                if (not valid and clear_sessions)
                {
                    char socket_file[128];
                    format_to(socket_file, "/tmp/kakoune/{}/{}", username, session);
                    unlink(socket_file);
                }
            }
            return 0;
        }

        if (auto session = parser.get_switch("p"))
        {
            for (auto opt : { "c", "n", "s", "d", "e", "ro" })
            {
                if (parser.get_switch(opt))
                {
                    write_stderr(format("error: -{} is incompatible with -p\n", opt));
                    return -1;
                }
            }
            return run_pipe(*session);
        }

        auto init_command = parser.get_switch("e").value_or(StringView{});
        const UIType ui_type = parse_ui_type(parser.get_switch("ui").value_or("ncurses"));

        if (auto keys = parser.get_switch("f"))
        {
            if (parser.get_switch("ro"))
            {
                write_stderr("error: -ro is incompatible with -f\n");
                return -1;
            }

            Vector<StringView> files;
            for (size_t i = 0; i < parser.positional_count(); ++i)
                files.emplace_back(parser[i]);

            return run_filter(*keys, init_command, files,
                              (bool)parser.get_switch("q"));
        }

        if (auto server_session = parser.get_switch("c"))
        {
            for (auto opt : { "n", "s", "d", "ro" })
            {
                if (parser.get_switch(opt))
                {
                    write_stderr(format("error: -{} is incompatible with -c\n", opt));
                    return -1;
                }
            }
            String new_files;
            for (auto name : parser)
                new_files += format("edit '{}';", escape(real_path(name), "'", '\\'));

            return run_client(*server_session, new_files + init_command, ui_type);
        }
        else
        {
            ByteCoord target_coord;
            Vector<StringView> files;
            for (auto& name : parser)
            {
                if (not name.empty() and name[0_byte] == '+')
                {
                    auto colon = find(name, ':');
                    if (auto line = str_to_int_ifp({name.begin()+1, colon}))
                    {
                        target_coord.line = *line - 1;
                        if (colon != name.end())
                            target_coord.column = str_to_int_ifp({colon+1, name.end()}).value_or(1) - 1;

                        continue;
                    }
                }

                files.emplace_back(name);
            }

            StringView session = parser.get_switch("s").value_or(StringView{});
            try
            {
                return run_server(session, init_command,
                                  (bool)parser.get_switch("n"),
                                  (bool)parser.get_switch("d"),
                                  (bool)parser.get_switch("ro"),
                                  ui_type, files, target_coord);
            }
            catch (convert_to_client_mode& convert)
            {
                raise(SIGTSTP);
                return run_client(convert.session,
                                  format("try %^buffer '{}'; select '{}'^; echo converted to client only mode",
                                         escape(convert.buffer_name, "'^", '\\'), convert.selections), ui_type);
            }
        }
    }
    catch (Kakoune::parameter_error& error)
    {
        write_stderr(format("Error while parsing parameters: {}\n"
                            "Valid switches:\n"
                            "{}", error.what(),
                            generate_switches_doc(param_desc.switches)));
       return -1;
    }
    catch (startup_error& error)
    {
        write_stderr(format("Could not start kakoune: {}\n", error.what()));
        return -1;
    }
    catch (Kakoune::exception& error)
    {
        write_stderr(format("uncaught exception ({}):\n{}", typeid(error).name(), error.what()));
        return -1;
    }
    catch (std::exception& error)
    {
        write_stderr(format("uncaught exception ({}):\n{}", typeid(error).name(), error.what()));
        return -1;
    }
    catch (...)
    {
        write_stderr("uncaught exception");
        return -1;
    }
    return 0;
}
