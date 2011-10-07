#include "debug.hh"

#include "assert.hh"
#include "buffer_manager.hh"

namespace Kakoune
{

static Buffer& get_or_create_debug_buffer()
{
    static const std::string debug_buffer_name("*debug*");
    Buffer* buffer = BufferManager::instance().get_buffer(debug_buffer_name);

    if (not buffer)
        buffer = new Buffer(debug_buffer_name, Buffer::Type::Scratch);

    assert(buffer);
    return *buffer;
}

void write_debug(const std::string& str)
{
    Buffer& debug_buffer = get_or_create_debug_buffer();
    debug_buffer.insert(debug_buffer.end(), str);
}

}
