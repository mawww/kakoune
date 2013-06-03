#include "client_manager.hh"

#include "buffer_manager.hh"
#include "color_registry.hh"
#include "command_manager.hh"
#include "event_manager.hh"
#include "file.hh"
#include "user_interface.hh"
#include "window.hh"

namespace Kakoune
{

Client::Client(std::unique_ptr<UserInterface>&& ui,
               Window& window, String name)
    : m_user_interface(std::move(ui)),
      m_input_handler(*m_user_interface),
      m_name(std::move(name))
{
    kak_assert(not m_name.empty());
    context().change_editor(window);
}

ClientManager::ClientManager() = default;
ClientManager::~ClientManager() = default;

String ClientManager::generate_name() const
{
    for (int i = 0; true; ++i)
    {
        String name = "unnamed" + to_string(i);
        bool found = false;
        for (auto& client : m_clients)
        {
            if (client->m_name == name)
            {
                found = true;
                break;
            }
        }
        if (not found)
            return name;
    }
}

Client* ClientManager::create_client(std::unique_ptr<UserInterface>&& ui,
                                     const String& init_commands)
{
    Buffer& buffer = **BufferManager::instance().begin();
    Client* client = new Client{std::move(ui), get_unused_window_for_buffer(buffer),
                                generate_name()};
    m_clients.emplace_back(client);
    try
    {
        CommandManager::instance().execute(init_commands, client->context());
    }
    catch (Kakoune::runtime_error& error)
    {
        client->context().print_status({ error.what(), get_color("Error") });
        client->context().hooks().run_hook("RuntimeError", error.what(),
                                           client->context());
    }
    catch (Kakoune::client_removed&)
    {
        m_clients.pop_back();
        return nullptr;
    }

    client->m_user_interface->set_input_callback([client, this]() {
        try
        {
            client->m_input_handler.handle_available_inputs();
            client->context().window().forget_timestamp();
        }
        catch (Kakoune::runtime_error& error)
        {
            client->context().print_status({ error.what(), get_color("Error") });
            client->context().hooks().run_hook("RuntimeError", error.what(),
                                               client->context());
        }
        catch (Kakoune::client_removed&)
        {
            ClientManager::instance().remove_client(*client);
        }
        ClientManager::instance().redraw_clients();
    });
    redraw_clients();

    return client;
}

void ClientManager::remove_client(Client& client)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
    {
        if (it->get() == &client)
        {
             m_clients.erase(it);
             return;
        }
    }
    kak_assert(false);
}

Window& ClientManager::get_unused_window_for_buffer(Buffer& buffer)
{
    for (auto& w : m_windows)
    {
        if (&w->buffer() != &buffer)
           continue;

        auto it = std::find_if(m_clients.begin(), m_clients.end(),
                               [&](const std::unique_ptr<Client>& client)
                               { return &client->context().window() == w.get(); });
        if (it == m_clients.end())
        {
            w->forget_timestamp();
            return *w;
        }
    }
    m_windows.emplace_back(new Window(buffer));
    return *m_windows.back();
}

void ClientManager::ensure_no_client_uses_buffer(Buffer& buffer)
{
    for (auto& client : m_clients)
    {
        client->context().forget_jumps_to_buffer(buffer);

        if (&client->context().buffer() != &buffer)
            continue;

        if (client->context().editor().is_editing())
            throw runtime_error("client '" + client->m_name + "' is inserting in '" +
                                buffer.display_name() + '\'');

        // change client context to edit the first buffer which is not the
        // specified one. As BufferManager stores buffer according to last
        // access, this selects a sensible buffer to display.
        for (auto& buf : BufferManager::instance())
        {
            if (buf != &buffer)
            {
               Window& w = get_unused_window_for_buffer(*buf);
               client->context().change_editor(w);
               break;
            }
        }
    }
    auto end = std::remove_if(m_windows.begin(), m_windows.end(),
                              [&buffer](const std::unique_ptr<Window>& w)
                              { return &w->buffer() == &buffer; });
    m_windows.erase(end, m_windows.end());
}

void ClientManager::set_client_name(Client& client, String name)
{
    auto it = find_if(m_clients, [&name](std::unique_ptr<Client>& client)
                                 { return client->m_name == name; });
    if (it != m_clients.end() and it->get() != &client)
        throw runtime_error("name not unique: " + name);
    client.m_name = std::move(name);
}

Client& ClientManager::get_client(const Context& context)
{
    for (auto& client : m_clients)
    {
        if (&client->context() == &context)
            return *client;
    }
    throw runtime_error("no client for current context");
}

Client& ClientManager::get_client(const String& name)
{
    for (auto& client : m_clients)
    {
        if (client->m_name == name)
            return *client;
    }
    throw runtime_error("no client named: " + name);
}

static DisplayLine generate_status_line(Client& client)
{
    auto& context = client.context();
    auto pos = context.editor().main_selection().last();
    auto col = context.buffer().char_distance({pos.line, 0}, pos);

    std::ostringstream oss;
    oss << context.buffer().display_name()
        << " " << (int)pos.line+1 << ":" << (int)col+1;
    if (context.buffer().is_modified())
        oss << " [+]";
    if (context.input_handler().is_recording())
       oss << " [recording]";
    if (context.buffer().flags() & Buffer::Flags::New)
        oss << " [new file]";
    oss << " [" << context.editor().selections().size() << " sel]";
    if (context.editor().is_editing())
        oss << " [insert]";
    oss << " - " << client.name();
    return { oss.str(), get_color("StatusLine") };
}

void ClientManager::redraw_clients() const
{
    for (auto& client : m_clients)
    {
        Context& context = client->context();
        if (context.window().timestamp() != context.buffer().timestamp())
        {
            DisplayCoord dimensions = context.ui().dimensions();
            if (dimensions == DisplayCoord{0,0})
                return;
            context.window().set_dimensions(dimensions);
            context.window().update_display_buffer();;

            context.ui().draw(context.window().display_buffer(),
                              generate_status_line(*client));
        }
    }
}

}
