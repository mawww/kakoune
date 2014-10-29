#include "assert.hh"
#include "alias_registry.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "highlighters.hh"
#include "hook_manager.hh"
#include "keymap_manager.hh"
#include "ncurses.hh"
#include "option_manager.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "interned_string.hh"
#include "window.hh"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <unordered_map>
#include <locale>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace Kakoune;

void run_unit_tests();

String runtime_directory()
{
    char buffer[2048];
#if defined(__linux__) or defined(__CYGWIN__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
#elif defined(__APPLE__)
    uint32_t bufsize = 2048;
    _NSGetExecutablePath(buffer, &bufsize);
    char* canonical_path = realpath(buffer, nullptr);
    strncpy(buffer, canonical_path, 2048);
    free(canonical_path);
#else
# error "finding executable path is not implemented on this platform"
#endif
    char* ptr = strrchr(buffer, '/');
    if (not ptr)
        throw runtime_error("unable to determine runtime directory");
    return String(buffer, ptr) + "/../share/kak";
}

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
            "timestamp",
            [](StringView name, const Context& context)
            { return to_string(context.buffer().timestamp()); }
        }, {
            "selection",
            [](StringView name, const Context& context)
            { const Selection& sel = context.selections().main();
              return content(context.buffer(), sel); }
        }, {
            "selections",
            [](StringView name, const Context& context)
            { auto sels = context.selections_content();
              String res;
              for (size_t i = 0; i < sels.size(); ++i)
              {
                  res += escape(sels[i], ':', '\\');
                  if (i != sels.size() - 1)
                      res += ':';
              }
              return res; }
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
            [](StringView name, const Context& context) -> String
            { return context.main_sel_register_value(name.substr(4_byte)); }
        }, {
            "client_env_.+",
            [](StringView name, const Context& context) -> String
            { return context.client().get_env_var(name.substr(11_byte)); }
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
            [](StringView name, const Context& context)
            { return to_string(context.selections().main().cursor().line + 1); }
        }, {
            "cursor_column",
            [](StringView name, const Context& context)
            { return to_string(context.selections().main().cursor().column + 1); }
        }, {
            "cursor_char_column",
            [](StringView name, const Context& context)
            { auto coord = context.selections().main().cursor();
              return to_string(context.buffer()[coord.line].char_count_to(coord.column) + 1); }
        }, {
            "selection_desc",
            [](StringView name, const Context& context)
            { auto& sel = context.selections().main();
                auto beg = sel.min();
                return to_string(beg.line + 1) + '.' + to_string(beg.column + 1) + '+' +
                       to_string((int)context.buffer().distance(beg, sel.max())+1); }
        }, {
            "selections_desc",
            [](StringView name, const Context& context)
            {
                String res;
                for (auto& sel : context.selections())
                {
                    auto beg = sel.min();
                    if (not res.empty())
                        res += ':';
                    res += to_string(beg.line + 1) + '.' + to_string(beg.column + 1) + '+' +
                           to_string((int)context.buffer().distance(beg, sel.max())+1);
                }
                return res;
            }
        }, {
            "window_width",
            [](StringView name, const Context& context)
            { return to_string(context.window().dimensions().column); }
        }, {
            "window_height",
            [](StringView name, const Context& context)
            { return to_string(context.window().dimensions().line); }
    } };

    ShellManager& shell_manager = ShellManager::instance();
    for (auto& env_var : env_vars)
        shell_manager.register_env_var(env_var.name, env_var.func);
}

void register_registers()
{
    using StringList = std::vector<String>;
    static const struct {
        char name;
        StringList (*func)(const Context&);
    } dyn_regs[] = {
        { '%', [](const Context& context) { return StringList{{context.buffer().display_name()}}; } },
        { '.', [](const Context& context) { return context.selections_content(); } },
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
                std::vector<String> result;
                for (auto& sel : context.selections())
                    result.emplace_back(i < sel.captures().size() ? sel.captures()[i] : "");
                return result;
            });
    }
}

void create_local_client(const String& init_command)
{
    class LocalNCursesUI : public NCursesUI
    {
        ~LocalNCursesUI()
        {
            if (not ClientManager::instance().empty() and fork())
            {
                this->NCursesUI::~NCursesUI();
                puts("detached from terminal\n");
                exit(0);
            }
        }
    };

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

    UserInterface* ui = new LocalNCursesUI{};
    static Client* client = ClientManager::instance().create_client(
        std::unique_ptr<UserInterface>{ui}, get_env_vars(), init_command);
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
        auto client = connect_to(session,
                                 std::unique_ptr<UserInterface>{new NCursesUI{}},
                                 get_env_vars(),
                                 init_command);
        while (true)
            event_manager.handle_next_events();
    }
    catch (peer_disconnected&)
    {
        fputs("disconnected from server\n", stderr);
        return -1;
    }
    catch (connection_failed& e)
    {
        fputs(e.what(), stderr);
        return -1;
    }
    return 0;
}

int run_server(StringView session, StringView init_command,
               bool ignore_kakrc, bool daemon, memoryview<StringView> files)
{
    static bool terminate = false;
    if (daemon)
    {
        if (session.empty())
        {
            fputs("-d needs a session name to be specified with -s\n", stderr);
            return -1;
        }
        if (pid_t child = fork())
        {
            printf("Kakoune forked to background, for session '%s'\n"
                   "send SIGTERM to process %d for closing the session\n",
                   (const char*)session.zstr(), child);
            exit(0);
        }
        signal(SIGTERM, [](int) { terminate = true; });
    }

    StringRegistry      string_registry;
    EventManager        event_manager;
    GlobalOptions       global_options;
    GlobalHooks         global_hooks;
    GlobalKeymaps       global_keymaps;
    GlobalAliases       global_aliases;
    ShellManager        shell_manager;
    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    DefinedHighlighters defined_highlighters;
    FaceRegistry        face_registry;
    ClientManager       client_manager;

    run_unit_tests();

    register_env_vars();
    register_registers();
    register_commands();
    register_highlighters();

    write_debug("*** This is the debug buffer, where debug info will be written ***");

    Server server(session.empty() ? to_string(getpid()) : String{session});

    if (not ignore_kakrc) try
    {
        Context initialisation_context;
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
        Context empty_context;
        global_hooks.run_hook("KakBegin", "", empty_context);
    }

    if (not files.empty()) try
    {
        // create buffers in reverse order so that the first given buffer
        // is the most recently created one.
        for (auto& file : reversed(files))
        {
            if (not create_buffer_from_file(file))
                new Buffer(file, Buffer::Flags::New | Buffer::Flags::File);
        }
    }
    catch (Kakoune::runtime_error& error)
    {
         write_debug("error while opening command line files: "_str + error.what());
    }
    else
        new Buffer("*scratch*", Buffer::Flags::None);

    if (not daemon)
        create_local_client(init_command);

    while (not terminate and (not client_manager.empty() or daemon))
    {
        event_manager.handle_next_events();
        client_manager.clear_mode_trashes();
        buffer_manager.clear_buffer_trash();
        client_manager.redraw_clients();
    }

    {
        Context empty_context;
        global_hooks.run_hook("KakEnd", "", empty_context);
    }

    return 0;
}

int run_filter(StringView keystr, memoryview<StringView> files)
{
    StringRegistry  string_registry;
    GlobalOptions   global_options;
    GlobalHooks     global_hooks;
    GlobalKeymaps   global_keymaps;
    GlobalAliases   global_aliases;
    ShellManager    shell_manager;
    BufferManager   buffer_manager;
    RegisterManager register_manager;

    register_env_vars();
    register_registers();

    try
    {
        auto keys = parse_keys(keystr);

        auto apply_keys_to_buffer = [&](Buffer& buffer)
        {
            try
            {
                InputHandler input_handler{{ buffer, Selection{{0,0}, buffer.back_coord()} }};

                for (auto& key : keys)
                    input_handler.handle_key(key);
            }
            catch (Kakoune::runtime_error& err)
            {
                fprintf(stderr, "error while applying keys to buffer '%s': %s\n",
                        buffer.display_name().c_str(), err.what());
            }
        };

        for (auto& file : files)
        {
            Buffer* buffer = create_buffer_from_file(file);
            apply_keys_to_buffer(*buffer);
            write_buffer_to_file(*buffer, file + ".kak-out");
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
        fprintf(stderr, "error: %s\n", err.what());
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
            fprintf(stderr, "error while reading stdin\n");
            return -1;
        }
        command += String{buf, buf + count};
    }
    try
    {
        send_command(session, command);
    }
    catch (connection_failed& e)
    {
        fputs(e.what(), stderr);
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

    std::vector<String> params;
    for (size_t i = 1; i < argc; ++i)
        params.push_back(argv[i]);

    const ParameterDesc param_desc{
        SwitchMap{ { "c", { true, "connect to given session" } },
                   { "e", { true, "execute argument on initialisation" } },
                   { "n", { false, "do not source kakrc files on startup" } },
                   { "s", { true, "set session name" } },
                   { "d", { false, "run as a headless session (requires -s)" } },
                   { "p", { true, "just send stdin as commands to the given session" } },
                   { "f", { true, "act as a filter, executing given keys on given files" } } }
    };
    try
    {
        ParametersParser parser(params, param_desc);

        if (parser.has_option("p"))
        {
            for (auto opt : { "c", "n", "s", "d", "e" })
            {
                if (parser.has_option(opt))
                {
                    fprintf(stderr, "error: -%s makes not sense with -p\n", opt);
                    return -1;
                }
            }
            return run_pipe(parser.option_value("p"));
        }
        else if (parser.has_option("f"))
        {
            std::vector<StringView> files;
            for (size_t i = 0; i < parser.positional_count(); ++i)
                files.emplace_back(parser[i]);

             return run_filter(parser.option_value("f"), files);
        }

        String init_command;
        if (parser.has_option("e"))
            init_command = parser.option_value("e");

        if (parser.has_option("c"))
        {
            for (auto opt : { "n", "s", "d" })
            {
                if (parser.has_option(opt))
                {
                    fprintf(stderr, "error: -%s makes not sense with -c\n", opt);
                    return -1;
                }
            }
            return run_client(parser.option_value("c"), init_command);
        }
        else
        {
            std::vector<StringView> files;
            for (size_t i = 0; i < parser.positional_count(); ++i)
                files.emplace_back(parser[i]);
            StringView session;
            if (parser.has_option("s"))
                session = parser.option_value("s");

            return run_server(session, init_command,
                              parser.has_option("n"),
                              parser.has_option("d"),
                              files);
        }
    }
    catch (Kakoune::parameter_error& error)
    {
        printf("Error: %s\n"
               "Valid switches:\n"
               "%s",
               error.what(),
               generate_switches_doc(param_desc.switches).c_str());
       return -1;
    }
    catch (Kakoune::exception& error)
    {
        on_assert_failed(("uncaught exception:\n"_str + error.what()).c_str());
        return -1;
    }
    catch (std::exception& error)
    {
        on_assert_failed(("uncaught exception:\n"_str + error.what()).c_str());
        return -1;
    }
    catch (...)
    {
        on_assert_failed("uncaught exception");
        return -1;
    }
    return 0;
}
