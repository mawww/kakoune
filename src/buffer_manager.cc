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
        if (*it == &buffer)
        {
            m_buffers.erase(it);
            return;
        }
    }
    for (auto it = m_buffer_trash.begin(); it != m_buffer_trash.end(); ++it)
    {
        if (*it == &buffer)
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
        if (*it == &buffer)
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

void BufferManager::delete_buffer_if_exists(StringView name)
{
    if (Buffer* buf = get_buffer_ifp(name))
        delete_buffer(*buf);
}

Buffer* BufferManager::get_buffer_ifp(StringView name)
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

Buffer& BufferManager::get_buffer(StringView name)
{
    Buffer* res = get_buffer_ifp(name);
    if (not res)
        throw runtime_error("no such buffer '"_str + name + "'");
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

void BufferManager::backup_modified_buffers()
{
    for (auto& buf : m_buffers)
    {
        if ((buf->flags() & Buffer::Flags::File) and buf->is_modified())
            write_buffer_to_backup_file(*buf);
    }
}

CandidateList BufferManager::complete_buffer_name(StringView prefix,
                                                  ByteCount cursor_pos)
{
    auto real_prefix = prefix.substr(0, cursor_pos);
    const bool include_dirs = contains(real_prefix, '/');
    CandidateList result;
    CandidateList subsequence_result;
    for (auto& buffer : m_buffers)
    {
        String name = buffer->display_name();
        StringView match_name = name;
        if (not include_dirs and buffer->flags() & Buffer::Flags::File)
        {
            ByteCount pos = name.find_last_of('/');
            if (pos != (int)String::npos)
                match_name = name.substr(pos+1);
        }

        if (prefix_match(match_name, real_prefix))
            result.push_back(name);
        if (subsequence_match(name, real_prefix))
            subsequence_result.push_back(name);
    }
    return result.empty() ? subsequence_result : result;
}

void BufferManager::clear_buffer_trash()
{
    while (not m_buffer_trash.empty())
        delete m_buffer_trash.back().get();
}

}
