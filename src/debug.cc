#include "debug.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "string.hh"

namespace Kakoune
{

void write_debug(StringView str)
{
    if (not BufferManager::has_instance())
    {
        write(2, str.data(), (int)str.length());
        write(2, "\n", 1);
        return;
    }

    const StringView debug_buffer_name = "*debug*";
    if (Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name))
        buffer->insert(buffer->end(), str);
    else
    {
        String line = str + ((str.empty() or str.back() != '\n') ? "\n" : "");
        create_buffer_from_data(line, debug_buffer_name, Buffer::Flags::NoUndo);
    }
}

}
