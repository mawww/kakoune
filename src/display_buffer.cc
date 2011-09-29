#include "display_buffer.hh"

#include "assert.h"

namespace Kakoune
{

DisplayBuffer::DisplayBuffer()
{
}

DisplayBuffer::iterator DisplayBuffer::split(iterator atom, size_t pos_in_atom)
{
    assert(atom < end());
    assert(pos_in_atom < atom->content.length());
    DisplayAtom new_atom(atom->begin, atom->begin + pos_in_atom,
                         atom->content.substr(0, pos_in_atom),
                         atom->fg_color, atom->bg_color, atom->attribute);

    atom->begin = atom->begin + pos_in_atom;
    atom->content = atom->content.substr(pos_in_atom);
    return insert(atom, std::move(new_atom));
}

}
