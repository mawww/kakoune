#include "client_manager.hh"

#include "buffer_manager.hh"
#include "command_manager.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "ranges.hh"
#include "window.hh"

namespace Kakoune
{

ClientManager::ClientManager() = default;
ClientManager::~ClientManager()
{
    clear(true);
}

void ClientManager::clear(bool disconnect_clients)
{
    if (disconnect_clients)
    {
        while (not m_clients.empty())
            remove_client(*m_clients.front(), true, 0);
    }
    else
        m_clients.clear();
    m_client_trash.clear();

    for (auto& window : m_free_windows)
        window.window->run_hook_in_own_context(Hook::WinClose,
                                               window.window->buffer().name());
    m_free_windows.clear();
    m_window_trash.clear();
}

String ClientManager::generate_name() const
{
    for (int i = 0; true; ++i)
    {
        String name = format("client{}", i);
        if (not client_name_exists(name))
            return name;
    }
}

Client* ClientManager::create_client(std::unique_ptr<UserInterface>&& ui, int pid,
                                     String name, EnvVarMap env_vars, StringView init_cmds,
                                     Optional<BufferCoord> init_coord,
                                     Client::OnExitCallback on_exit)
{
    Buffer& buffer = BufferManager::instance().get_first_buffer();
    WindowAndSelections ws = get_free_window(buffer);
    Client* client = new Client{std::move(ui), std::move(ws.window),
                                std::move(ws.selections), pid,
                                std::move(env_vars),
                                name.empty() ? generate_name() : std::move(name),
                                std::move(on_exit)};
    m_clients.emplace_back(client);

    if (init_coord)
    {
        auto& selections = client->context().selections_write_only();
        selections = SelectionList(buffer, buffer.clamp(*init_coord));
        client->context().window().center_line(init_coord->line);
    }

    try
    {
        auto& context = client->context();
        context.hooks().run_hook(Hook::ClientCreate, context.name(), context);
        CommandManager::instance().execute(init_cmds, context);
    }
    catch (Kakoune::runtime_error& error)
    {
        client->context().print_status({error.what().str(), client->context().faces()["Error"]});
        client->context().hooks().run_hook(Hook::RuntimeError, error.what(),
                                           client->context());
    }

    // Do not return the client if it already got moved to the trash
    return contains(m_clients, client) ? client : nullptr;
}

bool ClientManager::process_pending_inputs()
{
    bool processed_some_input = false;
    while (true)
    {
        bool had_input = false;
        // Use index based iteration as a m_clients might get mutated during
        // client input processing, which would break iterator based iteration.
        // (its fine to skip a client if that happens as had_input will be true
        // if a client triggers client removal)
        for (int i = 0; i < m_clients.size(); )
        {
            if (not m_clients[i]->is_ui_ok())
            {
                remove_client(*m_clients[i], false, -1);
                continue;
            }
            had_input = m_clients[i]->process_pending_inputs() or had_input;
            processed_some_input |= had_input;
            ++i;
        }

        if (not had_input)
            break;
    }
    return processed_some_input;
}

bool ClientManager::has_pending_inputs() const
{
    return any_of(m_clients, [](auto&& c) { return c->has_pending_inputs(); });
}

void ClientManager::remove_client(Client& client, bool graceful, int status)
{
    auto it = find(m_clients, &client);
    if (it == m_clients.end())
    {
        kak_assert(contains(m_client_trash, &client));
        return;
    }

    m_client_trash.push_back(std::move(*it));
    m_clients.erase(it);

    auto& context = client.context();
    context.hooks().run_hook(Hook::ClientClose, context.name(), context);

    client.exit(status);

    if (not graceful and m_clients.empty())
        BufferManager::instance().backup_modified_buffers();
}

WindowAndSelections ClientManager::get_free_window(Buffer& buffer)
{
    kak_assert(contains(BufferManager::instance(), &buffer));
    auto it = find_if(m_free_windows | reverse(),
                      [&](const WindowAndSelections& ws)
                      { return &ws.window->buffer() == &buffer; });

    if (it == m_free_windows.rend())
        return { std::make_unique<Window>(buffer), { buffer, Selection{} } };

    it->window->force_redraw();
    WindowAndSelections res = std::move(*it);
    m_free_windows.erase(it.base()-1);
    res.selections.update();
    return res;
}

void ClientManager::add_free_window(std::unique_ptr<Window>&& window, SelectionList selections)
{
    if (not contains(BufferManager::instance(), &window->buffer()))
    {
        m_window_trash.push_back(std::move(window));
        return;
    }

    window->clear_display_buffer();
    m_free_windows.push_back({std::move(window), std::move(selections)});
}

void ClientManager::ensure_no_client_uses_buffer(Buffer& buffer)
{
    for (auto& client : m_clients)
        client->context().forget_buffer(buffer);

    Vector<std::unique_ptr<Window>> removed_windows;
    m_free_windows.erase(remove_if(m_free_windows,
                                   [&buffer, &removed_windows](WindowAndSelections& ws) {
                                       if (&ws.window->buffer() != &buffer)
                                           return false;
                                       removed_windows.push_back(std::move(ws.window));
                                       return true;
                                   }),
                         m_free_windows.end());

    for (auto&& removed_window : removed_windows)
    {
        removed_window->run_hook_in_own_context(Hook::WinClose,
                                                removed_window->buffer().name());
        m_window_trash.push_back(std::move(removed_window));
    }
}

void ClientManager::clear_window_trash()
{
    m_window_trash.clear();
}

void ClientManager::clear_client_trash()
{
    m_client_trash.clear();
}

bool ClientManager::client_name_exists(StringView name) const
{
    return const_cast<ClientManager*>(this)->get_client_ifp(name) != nullptr;
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
    throw runtime_error(format("no such client: '{}'", name));
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
