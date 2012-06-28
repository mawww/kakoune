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
    typedef std::unordered_map<String, safe_ptr<Buffer>> BufferMap;

    struct iterator : public BufferMap::const_iterator
    {
        typedef BufferMap::const_iterator parent_type;

        iterator() {}
        iterator(const parent_type& other) : parent_type(other) {}
        Buffer& operator*()  const { return *(parent_type::operator*().second); }
        Buffer* operator->() const { return parent_type::operator*().second.get(); }
    };

    ~BufferManager();

    void register_buffer(Buffer* buffer);
    void unregister_buffer(Buffer* buffer);

    iterator begin() const { return iterator(m_buffers.begin()); }
    iterator end() const { return iterator(m_buffers.end()); }
    size_t   count() const { return m_buffers.size(); }

    Buffer* get_buffer(const String& name);

    CandidateList complete_buffername(const String& prefix,
                                      size_t cursor_pos = String::npos);

private:
    BufferMap m_buffers;
};

}

#endif // buffer_manager_hh_INCLUDED
