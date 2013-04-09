#include "debug.hh"

#include "assert.hh"
#include "buffer_manager.hh"
#include "editor.hh"

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
    Buffer& debug_buffer = get_or_create_debug_buffer();
    Editor editor(debug_buffer);
    editor.select(debug_buffer.end()-1);
    editor.insert(str + "\n");
}

}
