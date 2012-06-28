#include "buffer_manager.hh"

#include "assert.hh"
#include "buffer.hh"
#include "exception.hh"

namespace Kakoune
{

struct name_not_unique : logic_error {};

BufferManager::~BufferManager()
{
    // delete remaining buffers
    while (not m_buffers.empty())
        delete m_buffers.begin()->second.get();
}

void BufferManager::register_buffer(Buffer* buffer)
{
    assert(buffer);
    const String& name = buffer->name();
    if (m_buffers.find(name) != m_buffers.end())
        throw name_not_unique();

    m_buffers[name] = safe_ptr<Buffer>(buffer);
}

void BufferManager::unregister_buffer(Buffer* buffer)
{
    assert(buffer);
    auto buffer_it = m_buffers.find(buffer->name());
    if (buffer_it != m_buffers.end())
    {
        assert(buffer_it->second == buffer);
        m_buffers.erase(buffer_it);
    }
}

Buffer* BufferManager::get_buffer(const String& name)
{
    if (m_buffers.find(name) == m_buffers.end())
        return nullptr;

    return m_buffers[name].get();
}

CandidateList BufferManager::complete_buffername(const String& prefix,
                                                 size_t cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    for (auto& buffer : m_buffers)
    {
        if (buffer.first.substr(0, real_prefix.length()) == real_prefix)
            result.push_back(buffer.first);
    }
    return result;
}

}
