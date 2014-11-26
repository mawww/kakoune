#include "client.hh"

#include "face_registry.hh"
#include "context.hh"
#include "buffer_manager.hh"
#include "user_interface.hh"
#include "file.hh"
#include "remote.hh"
#include "client_manager.hh"
#include "event_manager.hh"
#include "window.hh"

#include <signal.h>
#include <unistd.h>

namespace Kakoune
{

Client::Client(std::unique_ptr<UserInterface>&& ui,
               std::unique_ptr<Window>&& window,
               SelectionList selections,
               EnvVarMap env_vars,
               String name)
    : m_ui{std::move(ui)}, m_window{std::move(window)},
      m_input_handler{std::move(selections),
                      std::move(name)},
      m_env_vars(env_vars)
{
    context().set_client(*this);
    context().set_window(*m_window);

    m_window->options().register_watcher(*this);
    m_ui->set_ui_options(m_window->options()["ui_options"].get<UserInterface::Options>());
}

Client::~Client()
{
    m_window->options().unregister_watcher(*this);
}

void Client::handle_available_input(EventMode mode)
{
    if (mode == EventMode::Normal)
    {
        try
        {
            for (auto& key : m_pending_keys)
            {
                m_input_handler.handle_key(key);
                m_input_handler.clear_mode_trash();
            }
            m_pending_keys.clear();

            while (m_ui->is_key_available())
            {
                if (key == ctrl('c'))
                    killpg(getpgrp(), SIGINT);
                else
                {
                    m_input_handler.handle_key(m_ui->get_key());
                    m_input_handler.clear_mode_trash();
                }
            }
            context().window().forget_timestamp();
        }
        catch (Kakoune::runtime_error& error)
        {
            context().print_status({ error.what(), get_face("Error") });
            context().hooks().run_hook("RuntimeError", error.what(), context());
        }
        catch (Kakoune::client_removed&)
        {
            ClientManager::instance().remove_client(*this);
        }
    }
    else
    {
        Key key = m_ui->get_key();
        if (key == ctrl('c'))
            killpg(getpgrp(), SIGINT);
        else
            m_pending_keys.push_back(key);
    }
}

void Client::print_status(DisplayLine status_line)
{
    m_pending_status_line = std::move(status_line);
}

DisplayLine Client::generate_mode_line() const
{
    auto pos = context().selections().main().cursor();
    auto col = context().buffer()[pos.line].char_count_to(pos.column);

    DisplayLine status;
    Face info_face = get_face("Information");
    Face status_face = get_face("StatusLine");

    status.push_back({ context().buffer().display_name(), status_face });
    status.push_back({ " " + to_string((int)pos.line+1) + ":" + to_string((int)col+1) + " ", status_face });
    if (context().buffer().is_modified())
        status.push_back({ "[+]", info_face });
    if (m_input_handler.is_recording())
        status.push_back({ "[recording ("_str + m_input_handler.recording_reg() + ")]", info_face });
    if (context().buffer().flags() & Buffer::Flags::New)
        status.push_back({ "[new file]", info_face });
    if (context().are_user_hooks_disabled())
        status.push_back({ "[no-hooks]", info_face });
    if (context().buffer().flags() & Buffer::Flags::Fifo)
        status.push_back({ "[fifo]", info_face });
    status.push_back({ " ", status_face });
    for (auto& atom : m_input_handler.mode_line())
        status.push_back(std::move(atom));
    status.push_back({ " - " + context().name() + "@[" + Server::instance().session() + "]", status_face });

    return status;
}

void Client::change_buffer(Buffer& buffer)
{
    auto& client_manager = ClientManager::instance();
    m_window->options().unregister_watcher(*this);
    client_manager.add_free_window(std::move(m_window),
                                   std::move(context().selections()));
    WindowAndSelections ws = client_manager.get_free_window(buffer);

    m_window = std::move(ws.window);
    m_window->options().register_watcher(*this);
    m_ui->set_ui_options(m_window->options()["ui_options"].get<UserInterface::Options>());

    context().m_selections = std::move(ws.selections);
    context().set_window(*m_window);
    m_window->set_dimensions(ui().dimensions());

    m_window->hooks().run_hook("WinDisplay", buffer.name(), context());
}

void Client::redraw_ifn()
{
    DisplayLine mode_line = generate_mode_line();
    const bool buffer_changed = context().window().timestamp() != context().buffer().timestamp();
    const bool mode_line_changed = mode_line.atoms() != m_mode_line.atoms();
    const bool status_line_changed = m_status_line.atoms() != m_pending_status_line.atoms();
    if (buffer_changed or status_line_changed or mode_line_changed)
    {
        if (buffer_changed)
        {
            CharCoord dimensions = context().ui().dimensions();
            if (dimensions == CharCoord{0,0})
                return;
            context().window().set_dimensions(dimensions);
            context().window().update_display_buffer(context());
        }
        m_mode_line = std::move(mode_line);
        m_status_line = m_pending_status_line;
        context().ui().draw(context().window().display_buffer(),
                            m_status_line, m_mode_line);
    }
    context().ui().refresh();
}

static void reload_buffer(Context& context, StringView filename)
{
    CharCoord view_pos = context.window().position();
    ByteCoord cursor_pos = context.selections().main().cursor();
    Buffer* buf = create_buffer_from_file(filename);
    if (not buf)
        return;
    context.change_buffer(*buf);
    context.selections() = SelectionList{ *buf, buf->clamp(cursor_pos)};
    context.window().set_position(view_pos);
    context.print_status({ "'" + buf->display_name() + "' reloaded",
                           get_face("Information") });
}

void Client::check_buffer_fs_timestamp()
{
    Buffer& buffer = context().buffer();
    auto reload = context().options()["autoreload"].get<YesNoAsk>();
    if (not (buffer.flags() & Buffer::Flags::File) or reload == No)
        return;

    const String& filename = buffer.name();
    time_t ts = get_fs_timestamp(filename);
    if (ts == InvalidTime or ts == buffer.fs_timestamp())
        return;
    if (reload == Ask)
    {
        m_ui->info_show(
            "reload '" + buffer.display_name() + "' ?",
            "'" + buffer.display_name() + "' was modified externally\n"
            "press r or y to reload, k or n to keep",
            CharCoord{}, get_face("Information"), InfoStyle::Prompt);

        m_input_handler.on_next_key(KeymapMode::None,
                                   [this, filename](Key key, Context& context) {
            Buffer* buf = BufferManager::instance().get_buffer_ifp(filename);
            m_ui->info_hide();
            // buffer got deleted while waiting for the key, do nothing
            if (not buf)
                return;
            if (key == 'r' or key == 'y')
                reload_buffer(context, filename);
            else if (key == 'k' or key == 'n')
            {
                // reread timestamp in case the file was modified again
                buf->set_fs_timestamp(get_fs_timestamp(filename));
                print_status({ "'" + buf->display_name() + "' kept",
                               get_face("Information") });
            }
            else
            {
                print_status({ "'" + key_to_str(key) + "' is not a valid choice",
                               get_face("Error") });
                check_buffer_fs_timestamp();
            }
        });
    }
    else
        reload_buffer(context(), filename);
}

const String& Client::get_env_var(const String& name) const
{
    auto it = m_env_vars.find(name);
    static String empty{};
    if (it == m_env_vars.end())
        return empty;
    return it->second;
}

void Client::on_option_changed(const Option& option)
{
    if (option.name() == "ui_options")
        m_ui->set_ui_options(option.get<UserInterface::Options>());
}

}
