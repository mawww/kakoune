#include "client_manager.hh"

#include "event_manager.hh"
#include "buffer_manager.hh"
#include "command_manager.hh"

namespace Kakoune
{

struct ClientManager::Client
{
    Client(std::unique_ptr<UserInterface>&& ui, Window& window,
           String name)
        : user_interface(std::move(ui)),
          input_handler(*user_interface),
          name(std::move(name))
    {
        assert(not this->name.empty());
        context().change_editor(window);
    }
    Client(Client&&) = delete;
    Client& operator=(Client&& other) = delete;

    Context& context() { return input_handler.context(); }

    std::unique_ptr<UserInterface> user_interface;
    InputHandler  input_handler;
    String        name;
};

ClientManager::ClientManager() = default;
ClientManager::~ClientManager() = default;

String ClientManager::generate_name() const
{
    for (int i = 0; true; ++i)
    {
        String name = "unnamed" + int_to_str(i);
        bool found = false;
        for (auto& client : m_clients)
        {
            if (client->name == name)
            {
                found = true;
                break;
            }
        }
        if (not found)
            return name;
    }
}

void ClientManager::create_client(std::unique_ptr<UserInterface>&& ui,
                                  const String& init_commands)
{
    Buffer& buffer = **BufferManager::instance().begin();
    m_clients.emplace_back(new Client{std::move(ui), get_unused_window_for_buffer(buffer),
                                      generate_name()});
    Context*       context = &m_clients.back()->context();
    try
    {
        CommandManager::instance().execute(init_commands, *context);
    }
    catch (Kakoune::runtime_error& error)
    {
        context->print_status(error.description());
    }
    catch (Kakoune::client_removed&)
    {
        m_clients.pop_back();
        return;
    }

    context->ui().set_input_callback([context, this]() {
        try
        {
            context->input_handler().handle_available_inputs();
            context->window().forget_timestamp();
        }
        catch (Kakoune::runtime_error& error)
        {
            context->print_status(error.description());
        }
        catch (Kakoune::client_removed&)
        {
            ClientManager::instance().remove_client_by_context(*context);
        }
        ClientManager::instance().redraw_clients();
    });
    redraw_clients();
}

void ClientManager::remove_client_by_context(Context& context)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
    {
        if (&(*it)->context() == &context)
        {
             m_clients.erase(it);
             return;
        }
    }
    assert(false);
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

void ClientManager::set_client_name(Context& context, String name)
{
    auto it = find_if(m_clients, [&name](std::unique_ptr<Client>& client)
                                 { return client->name == name; });
    if (it != m_clients.end() and &(*it)->context() != &context)
        throw runtime_error("name not unique: " + name);

    for (auto& client : m_clients)
    {
        if (&client->context() == &context)
        {
            client->name = std::move(name);
            return;
        }
    }
    throw runtime_error("no client for current context");
}

String ClientManager::get_client_name(const Context& context)
{
    for (auto& client : m_clients)
    {
        if (&client->context() == &context)
            return client->name;
    }
    throw runtime_error("no client for current context");
}

Context& ClientManager::get_client_context(const String& name)
{
    auto it = find_if(m_clients, [&name](std::unique_ptr<Client>& client)
                                 { return client->name == name; });
    if (it != m_clients.end())
        return (*it)->context();
    throw runtime_error("no client named: " + name);
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
                              context.window().status_line());
        }
    }
}

}
