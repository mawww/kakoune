#include "client_manager.hh"

#include "buffer_manager.hh"
#include "command_manager.hh"
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
        String name = "unnamed" + to_string(i);
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
        client->context().print_status({ error.what(), get_face("Error") });
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
    for (auto it = m_free_windows.rbegin(), end = m_free_windows.rend();
         it != end; ++it)
    {
        auto& w = it->window;
        if (&w->buffer() == &buffer)
        {
            w->forget_timestamp();
            WindowAndSelections res = std::move(*it);
            m_free_windows.erase(it.base()-1);
            res.selections.update();
            return res;
        }
    }
    return WindowAndSelections{ std::unique_ptr<Window>{new Window{buffer}},
                                SelectionList{ buffer, Selection{} } };
}

void ClientManager::add_free_window(std::unique_ptr<Window>&& window, SelectionList selections)
{
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
                                buffer.display_name() + '\'');

        // change client context to edit the first buffer which is not the
        // specified one. As BufferManager stores buffer according to last
        // access, this selects a sensible buffer to display.
        for (auto& buf : BufferManager::instance())
        {
            if (buf != &buffer)
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
    auto it = find_if(m_clients, [&](const std::unique_ptr<Client>& client)
                                 { return client->context().name() == name; });
    return it == m_clients.end();
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
    Client* client = get_client_ifp(name);
    if (not client)
        throw runtime_error("no client named: " + name);
    return *client;
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
    auto real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    CandidateList subsequence_result;
    for (auto& client : m_clients)
    {
        const String& name = client->context().name();

        if (prefix_match(name, real_prefix))
            result.push_back(name);
        if (subsequence_match(name, real_prefix))
            subsequence_result.push_back(name);
    }
    return result.empty() ? subsequence_result : result;
}

}
