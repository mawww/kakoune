#ifndef buffer_manager_hh_INCLUDED
#define buffer_manager_hh_INCLUDED

#include "buffer.hh"
#include "vector.hh"

#include <memory>

namespace Kakoune
{

class BufferManager : public Singleton<BufferManager>
{
public:
    using BufferList = Vector<std::unique_ptr<Buffer>, MemoryDomain::BufferMeta>;
    using iterator = BufferList::const_iterator;

    ~BufferManager();

    Buffer* create_buffer(String name, Buffer::Flags flags, BufferLines lines, ByteOrderMark bom, EolFormat eolformat, FsStatus fs_status);

    void delete_buffer(Buffer& buffer);

    iterator begin() const { return m_buffers.cbegin(); }
    iterator end() const { return m_buffers.cend(); }
    size_t   count() const { return m_buffers.size(); }

    Buffer* get_buffer_ifp(StringView name);
    Buffer& get_buffer(StringView name);

    void arrange_buffers(ConstArrayView<String> first_ones);

    Buffer& get_first_buffer();

    void backup_modified_buffers();

    void clear_buffer_trash();
private:
    BufferList m_buffers;
    BufferList m_buffer_trash;
};

}

#endif // buffer_manager_hh_INCLUDED
