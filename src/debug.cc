#include "debug.hh" 

#include "buffer_manager.hh" 
#include "buffer_utils.hh" 
#include "utils.hh" 

namespace Kakoune
{

void write_to_debug_buffer(StringView str)
{
    if (not BufferManager::has_instance())
    {
        write(2, str);
        write(2, "\n");
        return;
    }

    constexpr StringView debug_buffer_name = "*debug*";
    // Try to ensure we keep an empty line at the end of the debug buffer
    // where the user can put its cursor to scroll with new messages
    const bool eol_back = not str.empty() and str.back() == '\n';
    if (Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name))
    {
        buffer->flags() &= ~Buffer::Flags::ReadOnly;
        auto restore = OnScopeEnd([buffer] { buffer->flags() |= Buffer::Flags::ReadOnly; });

        buffer->insert(buffer->back_coord(), eol_back ? str : str + "\n");
    }
    else
    {
        String line = str + (eol_back ? "\n" : "\n\n");
        create_buffer_from_string(
            debug_buffer_name.str(), Buffer::Flags::NoUndo | Buffer::Flags::Debug | Buffer::Flags::ReadOnly,
            line);
    }
}

}
