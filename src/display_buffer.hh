#ifndef display_buffer_hh_INCLUDED
#define display_buffer_hh_INCLUDED

#include <string>
#include <vector>

namespace Kakoune
{

typedef int Attribute;

enum Attributes
{
    Normal = 0,
    Underline = 1,
    Reverse = 2,
    Blink = 4,
    Bold = 8,
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

struct DisplayAtom
{
    std::string content;
    Color       fg_color;
    Color       bg_color;
    Attribute   attribute;

    DisplayAtom()
        : fg_color(Color::Default),
          bg_color(Color::Default),
          attribute(Attributes::Normal)
    {}
};

class DisplayBuffer
{
public:
    typedef std::vector<DisplayAtom> AtomList; 
    typedef AtomList::iterator iterator;
    typedef AtomList::const_iterator const_iterator;

    DisplayBuffer();

    void clear() { m_atoms.clear(); }
    void append(const DisplayAtom& atom) { m_atoms.push_back(atom); }

    iterator begin() { return m_atoms.begin(); }
    iterator end()   { return m_atoms.end(); }

    const_iterator begin() const { return m_atoms.begin(); }
    const_iterator end()   const { return m_atoms.end(); }
private:
    AtomList m_atoms;
};

}

#endif // display_buffer_hh_INCLUDED
