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

std::vector<String> Editor::selections_content() const
{
    std::vector<String> contents;
    for (auto& sel : m_selections)
        contents.push_back(m_buffer->string(sel.min(), m_buffer->char_next(sel.max())));
    return contents;
}

void Editor::check_invariant() const
{
#ifdef KAK_DEBUG
    kak_assert(not m_selections.empty());
    m_selections.check_invariant();
    buffer().check_invariant();
#endif
}

}
