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
#include "filters.hh"
#include "highlighters.hh"
#include "hook_manager.hh"
#include "ncurses.hh"
#include "option_manager.hh"
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
    return String(buffer, ptr);
}

void register_env_vars()
{
    ShellManager& shell_manager = ShellManager::instance();

    shell_manager.register_env_var("bufname",
                                   [](const String& name, const Context& context)
                                   { return context.buffer().display_name(); });
    shell_manager.register_env_var("timestamp",
                                   [](const String& name, const Context& context)
                                   { return to_string(context.buffer().timestamp()); });
    shell_manager.register_env_var("selection",
                                   [](const String& name, const Context& context)
                                   { const Range& sel = context.editor().main_selection();
                                     return content(context.buffer(), sel); });
    shell_manager.register_env_var("selections",
                                   [](const String& name, const Context& context) {
                                       auto sels = context.editor().selections_content();
                                       String res;
                                       for (size_t i = 0; i < sels.size(); ++i)
                                       {
                                           res += escape(sels[i], ':', '\\');
                                           if (i != sels.size() - 1)
                                               res += ':';
                                       }
                                       return res;
                                   });
    shell_manager.register_env_var("runtime",
                                   [](const String& name, const Context& context)
                                   { return runtime_directory(); });
    shell_manager.register_env_var("opt_.+",
                                   [](const String& name, const Context& context)
                                   { return context.options()[name.substr(4_byte)].get_as_string(); });
    shell_manager.register_env_var("reg_.+",
                                   [](const String& name, const Context& context)
                                   { return RegisterManager::instance()[name[4]].values(context)[0]; });
    shell_manager.register_env_var("socket",
                                   [](const String& name, const Context& context)
                                   { return Server::instance().filename(); });
    shell_manager.register_env_var("client",
                                   [](const String& name, const Context& context)
                                   { return context.client().name(); });
    shell_manager.register_env_var("cursor_line",
                                   [](const String& name, const Context& context)
                                   { return to_string(context.editor().main_selection().last().line + 1); });
    shell_manager.register_env_var("cursor_column",
                                   [](const String& name, const Context& context)
                                   { return to_string(context.editor().main_selection().last().column + 1); });
    shell_manager.register_env_var("selection_desc",
                                   [](const String& name, const Context& context)
                                   { auto& sel = context.editor().main_selection();
                                     auto beg = sel.min();
                                     return to_string(beg.line + 1) + ':' + to_string(beg.column + 1) + '+' +
                                            to_string((int)context.buffer().distance(beg, sel.max())+1); });
    shell_manager.register_env_var("window_width",
                                   [](const String& name, const Context& context)
                                   { return to_string(context.window().dimensions().column); });
    shell_manager.register_env_var("window_height",
                                   [](const String& name, const Context& context)
                                   { return to_string(context.window().dimensions().line); });
}

void register_registers()
{
    RegisterManager& register_manager = RegisterManager::instance();

    register_manager.register_dynamic_register('%', [](const Context& context) { return std::vector<String>(1, context.buffer().display_name()); });
    register_manager.register_dynamic_register('.', [](const Context& context) { return context.editor().selections_content(); });
    for (size_t i = 0; i < 10; ++i)
    {
        register_manager.register_dynamic_register('0'+i,
            [i](const Context& context) {
                std::vector<String> result;
                for (auto& sel : context.editor().selections())
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
    abort();
}

int kakoune(memoryview<String> params)
{
    ParametersParser parser(params, { { "c", true },
                                      { "e", true },
                                      { "n", false },
                                      { "s", true } });
    String init_command;
    if (parser.has_option("e"))
        init_command = parser.option_value("e");

    if (parser.has_option("c"))
    {
        for (auto opt : { "n", "s" })
        {
            if (parser.has_option(opt))
            {
                fprintf(stderr, "error: -%s makes not sense with -c", opt);
                return -1;
            }
        }
        try
        {
            EventManager event_manager;
            auto client = connect_to(parser.option_value("c"),
                                     std::unique_ptr<UserInterface>{new NCursesUI{}},
                                     init_command);
            while (true)
                event_manager.handle_next_events();
        }
        catch (peer_disconnected&)
        {
            fputs("disconnected from server", stderr);
            return -1;
        }
        return 0;
    }
    else
    {
        EventManager        event_manager;
        GlobalOptions       global_options;
        GlobalHooks         global_hooks;
        ShellManager        shell_manager;
        CommandManager      command_manager;
        BufferManager       buffer_manager;
        RegisterManager     register_manager;
        HighlighterRegistry highlighter_registry;
        FilterRegistry      filter_registry;
        ColorRegistry       color_registry;
        ClientManager       client_manager;

        run_unit_tests();

        register_env_vars();
        register_registers();
        register_commands();
        register_highlighters();
        register_filters();

        write_debug("*** This is the debug buffer, where debug info will be written ***");
        write_debug("pid: " + to_string(getpid()));
        write_debug("utf-8 test: é á ï");

        Server server(parser.has_option("s") ? parser.option_value("s") : to_string(getpid()));

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

        create_local_client(init_command);

        while (not client_manager.empty())
            event_manager.handle_next_events();

        {
            Context empty_context;
            global_hooks.run_hook("KakEnd", "", empty_context);
        }
        return 0;
    }
}

int main(int argc, char* argv[])
{
    try
    {
        setlocale(LC_ALL, "");

        signal(SIGSEGV, signal_handler);
        signal(SIGFPE,  signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTERM, signal_handler);

        std::vector<String> params;
        for (size_t i = 1; i < argc; ++i)
             params.push_back(argv[i]);

        kakoune(params);
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
