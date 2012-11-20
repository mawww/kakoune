#include "client_manager.hh"

#include "event_manager.hh"
#include "buffer_manager.hh"

namespace Kakoune
{

void ClientManager::create_client(std::unique_ptr<UserInterface>&& ui,
                                  Buffer& buffer, int event_fd)
{
    m_clients.emplace_back(std::move(ui), get_unused_window_for_buffer(buffer));

    InputHandler*  input_handler = m_clients.back().input_handler.get();
    Context*       context = m_clients.back().context.get();
    EventManager::instance().watch(event_fd, [input_handler, context, this](int fd) {
        try
        {
            input_handler->handle_available_inputs(*context);
            context->window().forget_timestamp();
        }
        catch (Kakoune::runtime_error& error)
        {
            context->print_status(error.description());
        }
        catch (Kakoune::client_removed&)
        {
            ClientManager::instance().remove_client_by_context(*context);
            EventManager::instance().unwatch(fd);
            close(fd);
        }
        ClientManager::instance().redraw_clients();
    });
    redraw_clients();
}

void ClientManager::remove_client_by_context(Context& context)
{
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
    {
        if (it->context.get() == &context)
        {
             m_clients.erase(it);
             return;
        }
    }
    assert(false);
}

Window& ClientManager::get_unused_window_for_buffer(Buffer& buffer) const
{
    for (auto& w : buffer.windows())
    {
        auto it = std::find_if(m_clients.begin(), m_clients.end(),
                               [&](const Client& client) {
                                   return &client.context->window() == w.get();
                               });
        if (it == m_clients.end())
        {
            w->forget_timestamp();
            return *w;
        }
    }
    return buffer.new_window();
}

void ClientManager::ensure_no_client_uses_buffer(Buffer& buffer)
{
    for (auto& client : m_clients)
    {
        client.context->forget_jumps_to_buffer(buffer);

        if (&client.context->buffer() != &buffer)
            continue;

        // change client context to edit the first buffer which is not the
        // specified one. As BufferManager stores buffer according to last
        // access, this selects a sensible buffer to display.
        for (auto& buf : BufferManager::instance())
        {
            if (buf != &buffer)
            {
               Window& w = get_unused_window_for_buffer(*buf);
               client.context->change_editor(w);
               break;
            }
        }
    }
}

void ClientManager::redraw_clients() const
{
    for (auto& client : m_clients)
    {
        Context& context = *client.context;
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
