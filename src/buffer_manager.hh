#ifndef buffer_manager_hh_INCLUDED
#define buffer_manager_hh_INCLUDED

#include "completion.hh"
#include "utils.hh"
#include "safe_ptr.hh"

namespace Kakoune
{

class Buffer;

class BufferManager : public Singleton<BufferManager>
{
public:
    using BufferList = Vector<SafePtr<Buffer>>;
    using iterator = BufferList::const_iterator;

    BufferManager();
    ~BufferManager();

    void register_buffer(Buffer& buffer);
    void unregister_buffer(Buffer& buffer);

    void delete_buffer(Buffer& buffer);

    iterator begin() const { return m_buffers.cbegin(); }
    iterator end() const { return m_buffers.cend(); }
    size_t   count() const { return m_buffers.size(); }

    Buffer* get_buffer_ifp(StringView name);
    Buffer& get_buffer(StringView name);

    void backup_modified_buffers();

    void clear_buffer_trash();
private:
    BufferList m_buffers;
    BufferList m_buffer_trash;
};

}

#endif // buffer_manager_hh_INCLUDED
