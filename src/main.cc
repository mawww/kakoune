#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "client_manager.hh"
#include "color_registry.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "file.hh"
#include "highlighters.hh"
#include "hook_manager.hh"
#include "ncurses.hh"
#include "option_manager.hh"
#include "keymap_manager.hh"
#include "parameters_parser.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "window.hh"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <unordered_map>
#include <locale>
#include <signal.h>

using namespace Kakoune;

void run_unit_tests();

String runtime_directory()
{
    char buffer[2048];
#if defined(__linux__) || defined(__CYGWIN__)
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
        String (*func)(const String&, const Context&);
    } env_vars[] = { {
            "bufname",
            [](const String& name, const Context& context)
            { return context.buffer().display_name(); }
        }, {
            "buffile",
            [](const String& name, const Context& context)
            { return context.buffer().name(); }
        }, {
            "timestamp",
            [](const String& name, const Context& context)
            { return to_string(context.buffer().timestamp()); }
        }, {
            "selection",
            [](const String& name, const Context& context)
            { const Selection& sel = context.selections().main();
              return content(context.buffer(), sel); }
        }, {
            "selections",
            [](const String& name, const Context& context)
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
            [](const String& name, const Context& context)
            { return runtime_directory(); }
        }, {
            "opt_.+",
            [](const String& name, const Context& context)
            { return context.options()[name.substr(4_byte)].get_as_string(); }
        }, {
            "reg_.+",
            [](const String& name, const Context& context) -> String
            { return RegisterManager::instance()[name[4]].values(context)[0]; }
        }, {
            "session",
            [](const String& name, const Context& context) -> String
            { return Server::instance().session(); }
        }, {
            "client",
            [](const String& name, const Context& context) -> String
            { return context.name(); }
        }, {
            "cursor_line",
            [](const String& name, const Context& context)
            { return to_string(context.selections().main().cursor().line + 1); }
        }, {
            "cursor_column",
            [](const String& name, const Context& context)
            { return to_string(context.selections().main().cursor().column + 1); }
        }, {
            "cursor_char_column",
            [](const String& name, const Context& context)
            { auto coord = context.selections().main().cursor();
              return to_string(context.buffer()[coord.line].char_count_to(coord.column) + 1); }
        }, {
            "selection_desc",
            [](const String& name, const Context& context)
            { auto& sel = context.selections().main();
                auto beg = sel.min();
                return to_string(beg.line + 1) + ':' + to_string(beg.column + 1) + '+' +
                       to_string((int)context.buffer().distance(beg, sel.max())+1); }
        }, {
            "window_width",
            [](const String& name, const Context& context)
            { return to_string(context.window().dimensions().column); }
        }, {
            "window_height",
            [](const String& name, const Context& context)
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
        { '#', [](const Context& context) { return StringList{{to_string((int)context.selections().size())}}; } },
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

    UserInterface* ui = new LocalNCursesUI{};
    static Client* client = ClientManager::instance().create_client(
        std::unique_ptr<UserInterface>{ui}, init_command);
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
    on_assert_failed(text);
    if (Server::has_instance())
        Server::instance().close_session();
    abort();
}

int run_client(const String& session, const String& init_command)
{
    try
    {
        EventManager event_manager;
        auto client = connect_to(session,
                                 std::unique_ptr<UserInterface>{new NCursesUI{}},
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

int kakoune(const ParametersParser& parser)
{
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
            send_command(parser.option_value("p"), command);
        }
        catch (connection_failed& e)
        {
            fputs(e.what(), stderr);
            return -1;
        }
        return 0;
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

    const bool daemon = parser.has_option("d");
    static bool terminate = false;
    if (daemon)
    {
        if (not parser.has_option("s"))
        {
            fputs("-d needs a session name to be specified with -s\n", stderr);
            return -1;
        }
        if (pid_t child = fork())
        {
            printf("Kakoune forked to background, for session '%s'\n"
                   "send SIGTERM to process %d for closing the session\n",
                   parser.option_value("s").c_str(), child);
            exit(0);
        }
        signal(SIGTERM, [](int) { terminate = true; });
    }

    EventManager        event_manager;
    GlobalOptions       global_options;
    GlobalHooks         global_hooks;
    GlobalKeymaps       global_keymaps;
    ShellManager        shell_manager;
    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    DefinedHighlighters defined_highlighters;
    ColorRegistry       color_registry;
    ClientManager       client_manager;

    run_unit_tests();

    register_env_vars();
    register_registers();
    register_commands();
    register_highlighters();

    write_debug("*** This is the debug buffer, where debug info will be written ***");
    write_debug("pid: " + to_string(getpid()));

    Server server(parser.has_option("s") ? parser.option_value("s") : to_string(getpid()));
    write_debug("session: " + server.session());

    if (not parser.has_option("n")) try
    {
        Context initialisation_context;
        command_manager.execute("source " + runtime_directory() + "/kakrc",
                                initialisation_context);
    }
    catch (Kakoune::runtime_error& error)
    {
        write_debug("error while parsing kakrc: "_str + error.what());
    }
    catch (Kakoune::client_removed&)
    {
        write_debug("error while parsing kakrc: asked to quit");
    }

    {
        Context empty_context;
        global_hooks.run_hook("KakBegin", "", empty_context);
    }

    if (parser.positional_count() != 0) try
    {
        // create buffers in reverse order so that the first given buffer
        // is the most recently created one.
        for (int i = parser.positional_count() - 1; i >= 0; --i)
        {
            const String& file = parser[i];
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
        event_manager.handle_next_events();

    {
        Context empty_context;
        global_hooks.run_hook("KakEnd", "", empty_context);
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
                   { "p", { true, "just send stdin as commands to the given session" } } }
    };
    try
    {
        kakoune(ParametersParser(params, param_desc));
    }
    catch (Kakoune::parameter_error& error)
    {
        printf("Error: %s\n"
               "Valid switches:\n"
               "%s",
               error.what(), generate_switches_doc(param_desc.switches).c_str());
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
