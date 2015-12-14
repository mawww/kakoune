#include "client.hh"

#include "face_registry.hh"
#include "context.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "user_interface.hh"
#include "file.hh"
#include "remote.hh"
#include "client_manager.hh"
#include "command_manager.hh"
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
      m_input_handler{std::move(selections), Context::Flags::None,
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

Optional<Key> Client::get_next_key(EventMode mode)
{
    if (not m_pending_keys.empty())
    {
        Key key = m_pending_keys.front();
        m_pending_keys.erase(m_pending_keys.begin());
        return key;
    }
    if (mode != EventMode::Pending and m_ui->is_key_available())
        return m_ui->get_key();
    return {};
}

void Client::handle_available_input(EventMode mode)
{
    if (mode == EventMode::Urgent)
    {
        Key key = m_ui->get_key();
        if (key == ctrl('c'))
            killpg(getpgrp(), SIGINT);
        else
            m_pending_keys.push_back(key);
        return;
    }

    try
    {
        try
        {
            while (Optional<Key> key = get_next_key(mode))
            {
                if (*key == ctrl('c'))
                    killpg(getpgrp(), SIGINT);
                else if (*key == Key::FocusIn)
                    context().hooks().run_hook("FocusIn", context().name(), context());
                else if (*key == Key::FocusOut)
                    context().hooks().run_hook("FocusOut", context().name(), context());
                else if (key->modifiers == Key::Modifiers::Resize)
                    force_redraw();
                else
                    m_input_handler.handle_key(*key);
            }
        }
        catch (Kakoune::runtime_error& error)
        {
            context().print_status({ error.what().str(), get_face("Error") });
            context().hooks().run_hook("RuntimeError", error.what(), context());
        }
    }
    catch (Kakoune::client_removed& removed)
    {
        ClientManager::instance().remove_client(*this, removed.graceful);
    }
}

void Client::print_status(DisplayLine status_line)
{
    m_pending_status_line = std::move(status_line);
}

DisplayLine Client::generate_mode_line() const
{
    DisplayLine modeline;
    try
    {
        const String& modelinefmt = context().options()["modelinefmt"].get<String>();

        modeline = parse_display_line(expand(modelinefmt, context()));
    }
    catch (runtime_error& err)
    {
        write_to_debug_buffer(format("Error while parsing modelinefmt: {}", err.what()));
        modeline.push_back({ "modelinefmt error, see *debug* buffer", get_face("Error") });
    }

    Face info_face = get_face("Information");

    if (context().buffer().is_modified())
        modeline.push_back({ "[+]", info_face });
    if (m_input_handler.is_recording())
        modeline.push_back({ format("[recording ({})]", m_input_handler.recording_reg()), info_face });
    if (context().buffer().flags() & Buffer::Flags::New)
        modeline.push_back({ "[new file]", info_face });
    if (context().user_hooks_disabled())
        modeline.push_back({ "[no-hooks]", info_face });
    if (context().buffer().flags() & Buffer::Flags::Fifo)
        modeline.push_back({ "[fifo]", info_face });
    modeline.push_back({ " " });
    for (auto& atom : m_input_handler.mode_line())
        modeline.push_back(std::move(atom));
    modeline.push_back({ format(" - {}@[{}]", context().name(), Server::instance().session()) });

    return modeline;
}

void Client::change_buffer(Buffer& buffer)
{
    if (m_buffer_reload_dialog_opened)
        close_buffer_reload_dialog();

    m_last_buffer = &m_window->buffer();

    auto& client_manager = ClientManager::instance();
    m_window->options().unregister_watcher(*this);
    client_manager.add_free_window(std::move(m_window),
                                   std::move(context().selections()));
    WindowAndSelections ws = client_manager.get_free_window(buffer);

    m_window = std::move(ws.window);
    m_window->options().register_watcher(*this);
    m_ui->set_ui_options(m_window->options()["ui_options"].get<UserInterface::Options>());

    context().selections_write_only() = std::move(ws.selections);
    context().set_window(*m_window);
    m_window->set_dimensions(ui().dimensions());

    m_window->hooks().run_hook("WinDisplay", buffer.name(), context());
}

void Client::redraw_ifn()
{
    Window& window = context().window();
    UserInterface& ui = context().ui();

    const bool needs_redraw = window.needs_redraw(context());
    if (needs_redraw)
        ui.draw(window.update_display_buffer(context()), get_face("Default"));

    DisplayLine mode_line = generate_mode_line();
    if (needs_redraw or
        m_status_line.atoms() != m_pending_status_line.atoms() or
        mode_line.atoms() != m_mode_line.atoms())
    {
        m_mode_line = std::move(mode_line);
        m_status_line = m_pending_status_line;

        ui.draw_status(m_status_line, m_mode_line, get_face("StatusLine"));
    }

    ui.refresh();
}

void Client::force_redraw()
{
    if (m_window)
        m_window->force_redraw();
}

void Client::reload_buffer()
{
    Buffer& buffer = context().buffer();
    reload_file_buffer(buffer);
    context().print_status({ format("'{}' reloaded", buffer.display_name()),
                             get_face("Information") });
}

void Client::on_buffer_reload_key(Key key)
{
    auto& buffer = context().buffer();

    if (key == 'y' or key == ctrl('m'))
        reload_buffer();
    else if (key == 'n' or key == Key::Escape)
    {
        // reread timestamp in case the file was modified again
        buffer.set_fs_timestamp(get_fs_timestamp(buffer.name()));
        print_status({ format("'{}' kept", buffer.display_name()),
                       get_face("Information") });
    }
    else
    {
        print_status({ format("'{}' is not a valid choice", key_to_str(key)),
                       get_face("Error") });
        m_input_handler.on_next_key(KeymapMode::None, [this](Key key, Context&){ on_buffer_reload_key(key); });
        return;
    }

    for (auto& client : ClientManager::instance())
    {
        if (&client->context().buffer() == &buffer and
            client->m_buffer_reload_dialog_opened)
            client->close_buffer_reload_dialog();
    }
}

void Client::close_buffer_reload_dialog()
{
    kak_assert(m_buffer_reload_dialog_opened);
    m_buffer_reload_dialog_opened = false;
    m_ui->info_hide();
    m_input_handler.reset_normal_mode();
}

void Client::check_if_buffer_needs_reloading()
{
    if (m_buffer_reload_dialog_opened)
        return;

    Buffer& buffer = context().buffer();
    auto reload = context().options()["autoreload"].get<Autoreload>();
    if (not (buffer.flags() & Buffer::Flags::File) or reload == Autoreload::No)
        return;

    const String& filename = buffer.name();
    timespec ts = get_fs_timestamp(filename);
    if (ts == InvalidTime or ts == buffer.fs_timestamp())
        return;
    if (reload == Autoreload::Ask)
    {
        m_ui->info_show(
            format("reload '{}' ?", buffer.display_name()),
            format("'{}' was modified externally\n"
                   "press <ret> or y to reload, <esc> or n to keep",
                   buffer.display_name()),
            CharCoord{}, get_face("Information"), InfoStyle::Prompt);

        m_buffer_reload_dialog_opened = true;
        m_input_handler.on_next_key(KeymapMode::None, [this](Key key, Context&){ on_buffer_reload_key(key); });
    }
    else
        reload_buffer();
}

StringView Client::get_env_var(StringView name) const
{
    auto it = m_env_vars.find(name);
    if (it == m_env_vars.end())
        return {};
    return it->value;
}

void Client::on_option_changed(const Option& option)
{
    if (option.name() == "ui_options")
        m_ui->set_ui_options(option.get<UserInterface::Options>());
}

}
