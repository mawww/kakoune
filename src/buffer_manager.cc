#include "buffer_manager.hh"

#include <cassert>

#include "buffer.hh"
#include "window.hh"
#include "exception.hh"

namespace Kakoune
{

struct name_not_unique : logic_error {};

BufferManager* BufferManager::ms_instance = nullptr;

BufferManager& BufferManager::instance()
{
    if (not ms_instance)
        ms_instance = new BufferManager();

    return *ms_instance;
}

void BufferManager::delete_instance()
{
    delete ms_instance;
    ms_instance = nullptr;
}

BufferManager::BufferManager()
{
}

void BufferManager::register_buffer(Buffer* buffer)
{
    assert(buffer);
    const std::string& name = buffer->name();
    if (m_buffers.find(name) != m_buffers.end())
        throw name_not_unique();

    m_buffers[name] = std::unique_ptr<Buffer>(buffer);
}

void BufferManager::delete_buffer(Buffer* buffer)
{
    assert(buffer);
    auto buffer_it = m_buffers.find(buffer->name());
    assert(buffer_it->second.get() == buffer);
    m_buffers.erase(buffer_it);
}

Buffer* BufferManager::get_buffer(const std::string& name)
{
    if (m_buffers.find(name) == m_buffers.end())
        return nullptr;

    return m_buffers[name].get();
}


}
