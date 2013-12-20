#include "debug.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"

namespace Kakoune
{

static Buffer& get_or_create_debug_buffer()
{
    static const String debug_buffer_name("*debug*");
    Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name);

    if (not buffer)
        buffer = new Buffer(debug_buffer_name, Buffer::Flags::NoUndo);

    kak_assert(buffer);
    return *buffer;
}

void write_debug(const String& str)
{
    Buffer& buffer = get_or_create_debug_buffer();
    buffer.insert(buffer.end(), str);
}

}
