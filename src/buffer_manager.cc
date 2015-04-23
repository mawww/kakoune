#include "buffer_manager.hh"

#include "assert.hh"
#include "buffer.hh"
#include "client_manager.hh"
#include "containers.hh"
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
    StringView name = buffer.name();
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
        if (it->get() == &buffer)
        {
            m_buffers.erase(it);
            return;
        }
    }
    for (auto it = m_buffer_trash.begin(); it != m_buffer_trash.end(); ++it)
    {
        if (it->get() == &buffer)
        {
            m_buffer_trash.erase(it);
            return;
        }
    }
    kak_assert(false);
}

void BufferManager::delete_buffer(Buffer& buffer)
{
    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it)
    {
        if (it->get() == &buffer)
        {
            if (ClientManager::has_instance())
                ClientManager::instance().ensure_no_client_uses_buffer(buffer);

            m_buffers.erase(it);
            m_buffer_trash.emplace_back(&buffer);
            return;
        }
    }
    kak_assert(false);
}

Buffer* BufferManager::get_buffer_ifp(StringView name)
{
    auto path = real_path(parse_filename(name));
    for (auto& buf : m_buffers)
    {
        if (buf->name() == name or
            (buf->flags() & Buffer::Flags::File and buf->name() == path))
            return buf.get();
    }
    return nullptr;
}

Buffer& BufferManager::get_buffer(StringView name)
{
    Buffer* res = get_buffer_ifp(name);
    if (not res)
        throw runtime_error("no such buffer '"_str + name + "'");
    return *res;
}

void BufferManager::set_last_used_buffer(Buffer& buffer)
{
    auto it = find_if(m_buffers, [&buffer](const SafePtr<Buffer>& p)
                                 { return p.get() == &buffer; });
    kak_assert(it != m_buffers.end());
    m_buffers.erase(it);
    m_buffers.emplace(m_buffers.begin(), &buffer);
}

void BufferManager::backup_modified_buffers()
{
    for (auto& buf : m_buffers)
    {
        if ((buf->flags() & Buffer::Flags::File) and buf->is_modified())
            write_buffer_to_backup_file(*buf);
    }
}

void BufferManager::clear_buffer_trash()
{
    while (not m_buffer_trash.empty())
        delete m_buffer_trash.back().get();
}

}
