#include "client_manager.hh"

#include "buffer_manager.hh"
#include "command_manager.hh"
#include "containers.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "user_interface.hh"
#include "window.hh"

namespace Kakoune
{

ClientManager::ClientManager() = default;
ClientManager::~ClientManager() = default;

String ClientManager::generate_name() const
{
    for (int i = 0; true; ++i)
    {
        String name = format("unnamed{}", i);
        if (validate_client_name(name))
            return name;
    }
}

Client* ClientManager::create_client(std::unique_ptr<UserInterface>&& ui,
                                     EnvVarMap env_vars,
                                     StringView init_commands)
{
    Buffer& buffer = **BufferManager::instance().begin();
    WindowAndSelections ws = get_free_window(buffer);
    Client* client = new Client{std::move(ui), std::move(ws.window),
                                std::move(ws.selections), std::move(env_vars),
                                generate_name()};
    m_clients.emplace_back(client);
    try
    {
        CommandManager::instance().execute(init_commands, client->context());
    }
    catch (Kakoune::runtime_error& error)
    {
        client->context().print_status({ error.what().str(), get_face("Error") });
        client->context().hooks().run_hook("RuntimeError", error.what(),
                                           client->context());
    }
    catch (Kakoune::client_removed&)
    {
        m_clients.pop_back();
        return nullptr;
    }

    client->ui().set_input_callback([client](EventMode mode) {
        client->handle_available_input(mode);
    });

    return client;
}

void ClientManager::handle_pending_inputs() const
{
    for (auto& client : m_clients)
        client->handle_available_input(EventMode::Pending);
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

WindowAndSelections ClientManager::get_free_window(Buffer& buffer)
{
    auto it = find_if(reversed(m_free_windows),
                      [&](const WindowAndSelections& ws)
                      { return &ws.window->buffer() == &buffer; });

    if (it == m_free_windows.rend())
        return { make_unique<Window>(buffer), { buffer, Selection{} } };

    it->window->forget_timestamp();
    WindowAndSelections res = std::move(*it);
    m_free_windows.erase(it.base()-1);
    res.selections.update();
    return res;
}

void ClientManager::add_free_window(std::unique_ptr<Window>&& window, SelectionList selections)
{
    window->clear_display_buffer();
    Buffer& buffer = window->buffer();
    m_free_windows.push_back({ std::move(window), SelectionList{ std::move(selections) }, buffer.timestamp() });
}

void ClientManager::ensure_no_client_uses_buffer(Buffer& buffer)
{
    for (auto& client : m_clients)
    {
        client->context().forget_jumps_to_buffer(buffer);

        if (&client->context().buffer() != &buffer)
            continue;

        if (client->context().is_editing())
            throw runtime_error("client '" + client->context().name() + "' is inserting in '" +
                                buffer.display_name() + "'");

        // change client context to edit the first buffer which is not the
        // specified one. As BufferManager stores buffer according to last
        // access, this selects a sensible buffer to display.
        for (auto& buf : BufferManager::instance())
        {
            if (buf.get() != &buffer)
            {
               client->context().change_buffer(*buf);
               break;
            }
        }
    }
    auto end = std::remove_if(m_free_windows.begin(), m_free_windows.end(),
                              [&buffer](const WindowAndSelections& ws)
                              { return &ws.window->buffer() == &buffer; });
    m_free_windows.erase(end, m_free_windows.end());
}

bool ClientManager::validate_client_name(StringView name) const
{
    return const_cast<ClientManager*>(this)->get_client_ifp(name) == nullptr;
}

Client* ClientManager::get_client_ifp(StringView name)
{
    for (auto& client : m_clients)
    {
        if (client->context().name() == name)
            return client.get();
    }
    return nullptr;
}

Client& ClientManager::get_client(StringView name)
{
    if (Client* client = get_client_ifp(name))
        return *client;
    throw runtime_error("no client named: " + name);
}

void ClientManager::redraw_clients() const
{
    for (auto& client : m_clients)
        client->redraw_ifn();
}

void ClientManager::clear_mode_trashes() const
{
    for (auto& client : m_clients)
        client->input_handler().clear_mode_trash();
}

CandidateList ClientManager::complete_client_name(StringView prefix,
                                                  ByteCount cursor_pos) const
{
    auto c = transformed(m_clients, [](const std::unique_ptr<Client>& c){ return c->context().name(); });
    return complete(prefix, cursor_pos, c, prefix_match, subsequence_match);
}

}
