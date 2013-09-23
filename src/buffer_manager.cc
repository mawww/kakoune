#include "buffer_manager.hh"

#include "assert.hh"
#include "buffer.hh"
#include "client_manager.hh"
#include "exception.hh"
#include "file.hh"
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

    m_buffers.emplace(m_buffers.begin(), &buffer);
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
    kak_assert(false);
}

void BufferManager::delete_buffer(Buffer& buffer)
{
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
    {
        if (*it == &buffer)
        {
            if (ClientManager::has_instance())
                ClientManager::instance().ensure_no_client_uses_buffer(buffer);
            delete it->get();
            return;
        }
    }
    kak_assert(false);
}

void BufferManager::delete_buffer_if_exists(const String& name)
{
    if (Buffer* buf = get_buffer_ifp(name))
        delete_buffer(*buf);
}

Buffer* BufferManager::get_buffer_ifp(const String& name)
{
    for (auto& buf : m_buffers)
    {
        if (buf->name() == name or
            (buf->flags() & Buffer::Flags::File and
             real_path(buf->name()) == real_path(parse_filename(name))))
            return buf.get();
    }
    return nullptr;
}

Buffer& BufferManager::get_buffer(const String& name)
{
    Buffer* res = get_buffer_ifp(name);
    if (not res)
        throw runtime_error("no such buffer '" + name + "'");
    return *res;
}

void BufferManager::set_last_used_buffer(Buffer& buffer)
{
    auto it = m_buffers.begin();
    while (*it != &buffer and it != m_buffers.end())
        ++it;
    kak_assert(it != m_buffers.end());
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
        String name = buffer->display_name();
        if (prefix_match(name, real_prefix))
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
                String name = buffer->display_name();
                if (boost::regex_search(name.begin(), name.end(), ex))
                    result.push_back(escape(name));
            }
        }
        catch (boost::regex_error& err) {}
    }
    return result;
}

}
