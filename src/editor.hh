#ifndef editor_hh_INCLUDED
#define editor_hh_INCLUDED

#include "buffer.hh"
#include "dynamic_selection_list.hh"
#include "memoryview.hh"

namespace Kakoune
{

namespace InputModes { class Insert; }

class Register;

// An Editor is a buffer mutator
//
// The Editor class provides methods to manipulate a set of selections
// and to use these selections to mutate it's buffer.
class Editor : public SafeCountable
{
public:
    typedef std::function<void (const Buffer&, SelectionList&)>  Selector;

    Editor(Buffer& buffer);
    virtual ~Editor() {}

    Buffer& buffer() const { return *m_buffer; }

    const SelectionList& selections() const { return m_selections; }
    SelectionList& selections() { return m_selections; }
    std::vector<String>  selections_content() const;

private:
    friend struct scoped_edition;
    friend class InputModes::Insert;

    void check_invariant() const;

    safe_ptr<Buffer>         m_buffer;
    DynamicSelectionList     m_selections;
};

}

#endif // editor_hh_INCLUDED

