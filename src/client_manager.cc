#include "client_manager.hh"

#include "buffer_manager.hh"
#include "command_manager.hh"
#include "containers.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "window.hh"

namespace Kakoune
{

ClientManager::ClientManager() = default;
ClientManager::~ClientManager()
{
    clear();
}

void ClientManager::clear()
{
    // So that clients destructor find the client manager empty
    // so that local UI does not fork.
    ClientList clients = std::move(m_clients);
    clients.clear();
    m_client_trash.clear();
    m_free_windows.clear();
    m_window_trash.clear();
}

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
                                     EnvVarMap env_vars, StringView init_cmds,
                                     Optional<BufferCoord> init_coord)
{
    Buffer& buffer = BufferManager::instance().get_first_buffer();
    WindowAndSelections ws = get_free_window(buffer);
    Client* client = new Client{std::move(ui), std::move(ws.window),
                                std::move(ws.selections), std::move(env_vars),
                                generate_name()};
    m_clients.emplace_back(client);

    if (init_coord)
    {
        auto& selections = client->context().selections_write_only();
        selections = SelectionList(buffer, buffer.clamp(*init_coord));
        client->context().window().center_line(init_coord->line);
    }

    try
    {
        CommandManager::instance().execute(init_cmds, client->context());
    }
    catch (Kakoune::runtime_error& error)
    {
        client->context().print_status({ error.what().str(), get_face("Error") });
        client->context().hooks().run_hook("RuntimeError", error.what(),
                                           client->context());
    }

    // Do not return the client if it already got moved to the trash
    return contains(m_clients, client) ? client : nullptr;
}

void ClientManager::process_pending_inputs() const
{
    while (true)
    {
        bool had_input = false;
        // Use index based iteration as a m_clients might get mutated during
        // client input processing, which would break iterator based iteration.
        // (its fine to skip a client if that happens as had_input will be true
        // if a client triggers client removal)
        for (int i = 0; i < m_clients.size(); ++i)
            had_input = m_clients[i]->process_pending_inputs() or had_input;

        if (not had_input)
            break;
    }
}

void ClientManager::remove_client(Client& client, bool graceful)
{
    auto it = find(m_clients, &client);
    if (it == m_clients.end())
    {
        kak_assert(contains(m_client_trash, &client));
        return;
    }
    m_client_trash.push_back(std::move(*it));
    m_clients.erase(it);

    if (not graceful and m_clients.empty())
        BufferManager::instance().backup_modified_buffers();
}

WindowAndSelections ClientManager::get_free_window(Buffer& buffer)
{
    auto it = find_if(m_free_windows | reverse(),
                      [&](const WindowAndSelections& ws)
                      { return &ws.window->buffer() == &buffer; });

    if (it == m_free_windows.rend())
        return { make_unique<Window>(buffer), { buffer, Selection{} } };

    it->window->force_redraw();
    WindowAndSelections res = std::move(*it);
    m_free_windows.erase(it.base()-1);
    res.selections.update();
    return res;
}

void ClientManager::add_free_window(std::unique_ptr<Window>&& window, SelectionList selections)
{
    window->clear_display_buffer();
    m_free_windows.push_back({ std::move(window), SelectionList{ std::move(selections) } });
}

void ClientManager::ensure_no_client_uses_buffer(Buffer& buffer)
{
    for (auto& client : m_clients)
    {
        auto& context = client->context();
        context.jump_list().forget_buffer(buffer);
        if (client->last_buffer() == &buffer)
            client->set_last_buffer(nullptr);

        if (&context.buffer() != &buffer)
            continue;

        if (context.is_editing())
            context.input_handler().reset_normal_mode();

        Buffer* last = client->last_buffer();
        context.change_buffer(last ? *last : BufferManager::instance().get_first_buffer());
    }
    auto end = std::remove_if(m_free_windows.begin(), m_free_windows.end(),
                              [&buffer](const WindowAndSelections& ws)
                              { return &ws.window->buffer() == &buffer; });

    for (auto it = end; it != m_free_windows.end(); ++it)
        m_window_trash.push_back(std::move(it->window));

    m_free_windows.erase(end, m_free_windows.end());
}

void ClientManager::clear_window_trash()
{
    m_window_trash.clear();
}

void ClientManager::clear_client_trash()
{
    m_client_trash.clear();
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
    throw runtime_error(format("no client named '{}'", name));
}

void ClientManager::redraw_clients() const
{
    for (auto& client : m_clients)
        client->redraw_ifn();
}

CandidateList ClientManager::complete_client_name(StringView prefix,
                                                  ByteCount cursor_pos) const
{
    auto c = m_clients | transform([](const std::unique_ptr<Client>& c) -> const String&
                                   { return c->context().name(); });
    return complete(prefix, cursor_pos, c);
}

}
