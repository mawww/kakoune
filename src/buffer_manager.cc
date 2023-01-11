#include "buffer_manager.hh"

#include "assert.hh"
#include "buffer.hh"
#include "client_manager.hh"
#include "exception.hh"
#include "file.hh"
#include "ranges.hh"
#include "string.hh"

namespace Kakoune
{

BufferManager::~BufferManager()
{
    // Move buffers to avoid running BufClose with buffers remaining in that list
    BufferList buffers = std::move(m_buffers);

    for (auto& buffer : buffers)
        buffer->on_unregistered();

    // Make sure not clients exists
    if (ClientManager::has_instance())
        ClientManager::instance().clear(true);
}

Buffer* BufferManager::create_buffer(String name, Buffer::Flags flags, BufferLines lines, ByteOrderMark bom, EolFormat eolformat, FsStatus fs_status)
{
    auto path = real_path(parse_filename(name));
    for (auto& buf : m_buffers)
    {
        if (buf->name() == name or
            (buf->flags() & Buffer::Flags::File and buf->name() == path))
            throw runtime_error{"buffer name is already in use"};
    }

    m_buffers.push_back(std::make_unique<Buffer>(std::move(name), flags, lines, bom, eolformat, fs_status));
    auto* buffer = m_buffers.back().get();
    buffer->on_registered();

    if (contains(m_buffer_trash, buffer))
        throw runtime_error{"buffer got removed during its creation"};

    return buffer;
}

void BufferManager::delete_buffer(Buffer& buffer)
{
    auto it = find_if(m_buffers, [&](auto& p) { return p.get() == &buffer; });
    if (it == m_buffers.end()) // we might be trying to recursively delete this buffer
        return;

    m_buffer_trash.emplace_back(std::move(*it));
    m_buffers.erase(it);

    if (ClientManager::has_instance())
        ClientManager::instance().ensure_no_client_uses_buffer(buffer);

    buffer.on_unregistered();
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
        throw runtime_error{format("no such buffer '{}'", name)};
    return *res;
}

Buffer& BufferManager::get_first_buffer()
{
    if (all_of(m_buffers, [](auto& b) { return (b->flags() & Buffer::Flags::Debug); }))
        create_buffer("*scratch*", Buffer::Flags::None,
                      {StringData::create({"*** this is a *scratch* buffer which won't be automatically saved ***\n"}),
                       StringData::create({"*** use it for notes or open a file buffer with the :edit command ***\n"})},
                      ByteOrderMark::None, EolFormat::Lf, {InvalidTime, {}, {}});

    return *m_buffers.back();
}

void BufferManager::backup_modified_buffers()
{
    for (auto& buf : m_buffers)
    {
        if ((buf->flags() & Buffer::Flags::File) and buf->is_modified()
            and not (buf->flags() & Buffer::Flags::ReadOnly))
            write_buffer_to_backup_file(*buf);
    }
}

void BufferManager::clear_buffer_trash()
{
    m_buffer_trash.clear();
}

void BufferManager::arrange_buffers(ConstArrayView<String> first_ones)
{
    Vector<size_t> indices;
    for (const auto& name : first_ones)
    {
        auto it = find_if(m_buffers, [&](auto& buf) { return buf->name() == name or buf->display_name() == name; });
        if (it == m_buffers.end())
            throw runtime_error{format("no such buffer '{}'", name)};
        size_t index = it - m_buffers.begin();
        if (contains(indices, index))
            throw runtime_error{format("buffer '{}' appears more than once", name)};
        indices.push_back(index);
    }

    BufferList res;
    for (size_t index : indices)
        res.push_back(std::move(m_buffers[index]));
    for (auto& buf : m_buffers)
    {
        if (buf)
            res.push_back(std::move(buf));
    }
    m_buffers = std::move(res);
}

}
