#include "debug.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "string.hh"

namespace Kakoune
{

void write_debug(StringView str)
{
    if (not BufferManager::has_instance())
    {
        fprintf(stderr, "%s\n", (const char*)str.zstr());
        return;
    }

    static const String debug_buffer_name("*debug*");
    Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name);

    if (not buffer)
        buffer = new Buffer(debug_buffer_name, Buffer::Flags::NoUndo);

    kak_assert(buffer);
    buffer->insert(buffer->end(), str);
}

}
