#ifndef editor_hh_INCLUDED
#define editor_hh_INCLUDED

#include "buffer.hh"
#include "dynamic_selection_list.hh"
#include "memoryview.hh"

namespace Kakoune
{

// An Editor is a to be removed class from the past
class Editor : public SafeCountable
{
public:
    Editor(Buffer& buffer)
    : m_buffer(&buffer),
      m_selections(buffer, {BufferCoord{}})
    {}

    virtual ~Editor() {}

    Buffer& buffer() const { return *m_buffer; }

    const SelectionList& selections() const { return m_selections; }
    SelectionList& selections() { return m_selections; }

private:
    safe_ptr<Buffer>         m_buffer;
    DynamicSelectionList     m_selections;
};

}

#endif // editor_hh_INCLUDED

