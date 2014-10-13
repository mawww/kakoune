#ifndef buffer_manager_hh_INCLUDED
#define buffer_manager_hh_INCLUDED

#include "completion.hh"
#include "utils.hh"
#include "safe_ptr.hh"

#include <unordered_map>

namespace Kakoune
{

class Buffer;

class BufferManager : public Singleton<BufferManager>
{
public:
    using BufferList = std::vector<safe_ptr<Buffer>>;
    using iterator = BufferList::const_iterator;

    ~BufferManager();

    void register_buffer(Buffer& buffer);
    void unregister_buffer(Buffer& buffer);

    void delete_buffer(Buffer& buffer);
    void delete_buffer_if_exists(StringView name);

    iterator begin() const { return m_buffers.cbegin(); }
    iterator end() const { return m_buffers.cend(); }
    size_t   count() const { return m_buffers.size(); }

    Buffer* get_buffer_ifp(StringView name);
    Buffer& get_buffer(StringView name);
    void    set_last_used_buffer(Buffer& buffer);

    void backup_modified_buffers();

    CandidateList complete_buffer_name(StringView prefix,
                                       ByteCount cursor_pos = -1);

    void clear_buffer_trash();
private:
    BufferList m_buffers;
    BufferList m_buffer_trash;
};

}

#endif // buffer_manager_hh_INCLUDED
