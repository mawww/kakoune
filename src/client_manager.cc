#include "client_manager.hh"

namespace Kakoune
{

void ClientManager::add_client(Client&& client)
{
    m_clients.emplace_back(std::move(client));
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

}
