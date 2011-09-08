#ifndef buffer_manager_hh_INCLUDED
#define buffer_manager_hh_INCLUDED

#include "buffer.hh"

#include <unordered_map>
#include <memory>

namespace Kakoune
{

class BufferManager
{
public:
    void register_buffer(Buffer* buffer);
    void delete_buffer(Buffer* buffer);

    Buffer* get_buffer(const std::string& name);

    static BufferManager& instance();
    static void delete_instance();

private:
    BufferManager();
    static BufferManager* ms_instance;

    std::unordered_map<std::string, std::unique_ptr<Buffer>> m_buffers;
};

}

#endif // buffer_manager_hh_INCLUDED
