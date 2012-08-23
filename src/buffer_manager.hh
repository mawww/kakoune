#ifndef buffer_manager_hh_INCLUDED
#define buffer_manager_hh_INCLUDED

#include "completion.hh"
#include "utils.hh"

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

    iterator begin() const { return m_buffers.cbegin(); }
    iterator end() const { return m_buffers.cend(); }
    size_t   count() const { return m_buffers.size(); }

    Buffer* get_buffer(const String& name);

    CandidateList complete_buffername(const String& prefix,
                                      CharCount cursor_pos = -1);

private:
    BufferList m_buffers;
};

}

#endif // buffer_manager_hh_INCLUDED
