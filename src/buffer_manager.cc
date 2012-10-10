#include "buffer_manager.hh"

#include "assert.hh"
#include "buffer.hh"
#include "exception.hh"
#include "string.hh"

namespace Kakoune
{

struct name_not_unique : logic_error {};

BufferManager::~BufferManager()
{
    // delete remaining buffers
    while (not m_buffers.empty())
        delete m_buffers.front().get();
}

void BufferManager::register_buffer(Buffer& buffer)
{
    const String& name = buffer.name();
    for (auto& buf : m_buffers)
    {
        if (buf->name() == name)
            throw name_not_unique();
    }

    m_buffers.push_back(safe_ptr<Buffer>(&buffer));
}

void BufferManager::unregister_buffer(Buffer& buffer)
{
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
    {
        if (*it == &buffer)
        {
            m_buffers.erase(it);
            return;
        }
    }
    assert(false);
}

Buffer* BufferManager::get_buffer(const String& name)
{
    for (auto& buf : m_buffers)
    {
        if (buf->name() == name)
            return buf.get();
    }
    return nullptr;
}

void BufferManager::set_last_used_buffer(Buffer& buffer)
{
    auto it = m_buffers.begin();
    while (*it != &buffer and it != m_buffers.end())
        ++it;
    assert(it != m_buffers.end());
    m_buffers.erase(it);
    m_buffers.emplace(m_buffers.begin(), &buffer);
}

CandidateList BufferManager::complete_buffername(const String& prefix,
                                                 ByteCount cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    for (auto& buffer : m_buffers)
    {
        const String& name = buffer->name();
        if (name.substr(0, real_prefix.length()) == real_prefix)
            result.push_back(escape(name));
    }
    // no prefix completion found, check regex matching
    if (result.empty())
    {
        try
        {
            Regex ex(real_prefix.begin(), real_prefix.end());
            for (auto& buffer : m_buffers)
            {
                const String& name = buffer->name();
                if (boost::regex_search(name.begin(), name.end(), ex))
                    result.push_back(escape(name));
            }
        }
        catch (boost::regex_error& err) {}
    }
    return result;
}

}
