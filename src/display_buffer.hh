#ifndef display_buffer_hh_INCLUDED
#define display_buffer_hh_INCLUDED

#include <string>
#include <vector>

#include "buffer.hh"

namespace Kakoune
{

typedef int Color;
typedef int Attribute;

enum Attributes
{
    UNDERLINE = 1
};

struct DisplayAtom
{
    std::string content;
    Color       fg_color;
    Color       bg_color;
    Attribute   attribute;

    DisplayAtom() : fg_color(0), bg_color(0), attribute(0) {}
};

class DisplayBuffer
{
public:
    typedef std::vector<DisplayAtom> AtomList; 
    typedef AtomList::iterator iterator;
    typedef AtomList::const_iterator const_iterator;

    DisplayBuffer();

    LineAndColumn dimensions() const;

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
