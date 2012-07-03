#ifndef display_buffer_hh_INCLUDED
#define display_buffer_hh_INCLUDED

#include <vector>

#include "string.hh"
#include "line_and_column.hh"
#include "buffer.hh"

namespace Kakoune
{

struct DisplayCoord : LineAndColumn<DisplayCoord>
{
    DisplayCoord(int line = 0, int column = 0)
        : LineAndColumn(line, column) {}

    template<typename T>
    explicit DisplayCoord(const LineAndColumn<T>& other)
        : LineAndColumn(other.line, other.column) {}
};

typedef int Attribute;

enum Attributes
{
    Normal = 0,
    Underline = 1,
    Reverse = 2,
    Blink = 4,
    Bold = 8,
    Final = 16
};

enum class Color
{
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

// A DisplayAtom is a string of text with it's display style.
//
// The DisplayAtom class references the buffer string it represents
// with it's begin/end iterators and may replace it with another
// text stored in the replacement_string field.
struct DisplayAtom
{
    const DisplayCoord&   coord()     const { return m_coord; }
    const BufferIterator& begin()     const { return m_begin; }
    const BufferIterator& end()       const { return m_end; }
    const Color&          fg_color()  const { return m_fg_color; }
    const Color&          bg_color()  const { return m_bg_color; }
    const Attribute&      attribute() const { return m_attribute; }

    enum ContentMode
    {
        BufferText,
        ReplacementText
    };
    ContentMode    content_mode() const { return m_content_mode; }

    Color&         fg_color()  { return m_fg_color; }
    Color&         bg_color()  { return m_bg_color; }
    Attribute&     attribute() { return m_attribute; }

    String         content()    const;
    DisplayCoord   end_coord()  const;
    BufferIterator iterator_at(const DisplayCoord& coord) const;
    DisplayCoord   line_and_column_at(const BufferIterator& iterator) const;

    bool           splitable() const { return m_content_mode != ReplacementText; }

private:
    friend class DisplayBuffer;
    DisplayAtom(DisplayCoord coord,
                BufferIterator begin, BufferIterator end,
                Color fg_color = Color::Default,
                Color bg_color = Color::Default,
                Attribute attribute = Attributes::Normal)
        : m_content_mode(BufferText),
          m_coord(std::move(coord)),
          m_begin(std::move(begin)), m_end(std::move(end)),
          m_fg_color(fg_color),
          m_bg_color(bg_color),
          m_attribute(attribute)
    {}

    ContentMode    m_content_mode;

    DisplayCoord   m_coord;
    BufferIterator m_begin;
    BufferIterator m_end;
    Color          m_fg_color;
    Color          m_bg_color;
    Attribute      m_attribute;
    String         m_replacement_text;
};

// A DisplayBuffer is the visual content of a Window as a DisplayAtom list
//
// The DisplayBuffer class provides means to mutate and iterator on it's
// DisplayAtoms.
class DisplayBuffer
{
public:
    typedef std::list<DisplayAtom> AtomList;
    typedef AtomList::iterator iterator;
    typedef AtomList::const_iterator const_iterator;

    DisplayBuffer();

    void clear() { m_atoms.clear(); }
    iterator append(BufferIterator begin, BufferIterator end);
    iterator insert_empty_atom_before(iterator where);
    iterator split(iterator atom, const BufferIterator& pos);

    void replace_atom_content(iterator atom, const String& replacement);

    iterator begin() { return m_atoms.begin(); }
    iterator end()   { return m_atoms.end(); }

    const_iterator begin() const { return m_atoms.begin(); }
    const_iterator end()   const { return m_atoms.end(); }

    iterator atom_containing(const BufferIterator& where);
    iterator atom_containing(const BufferIterator& where, iterator start);

    const DisplayAtom& front() const { return m_atoms.front(); }
    const DisplayAtom& back()  const { return m_atoms.back(); }

    void check_invariant() const;
private:
    AtomList m_atoms;
};

}

#endif // display_buffer_hh_INCLUDED
