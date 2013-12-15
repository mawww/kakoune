#include "editor.hh"

#include "exception.hh"
#include "register.hh"
#include "register_manager.hh"
#include "utf8_iterator.hh"
#include "utils.hh"

#include <array>

namespace Kakoune
{

Editor::Editor(Buffer& buffer)
    : m_buffer(&buffer),
      m_selections(buffer, {BufferCoord{}})
{}

}
