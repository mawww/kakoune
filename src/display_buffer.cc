#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

DisplayBuffer::DisplayBuffer()
{
}

DisplayBuffer::iterator DisplayBuffer::split(iterator atom, size_t pos_in_atom)
{
    assert(atom < end());
    assert(pos_in_atom > 0);
    assert(pos_in_atom < atom->content.length());
    DisplayAtom new_atom(atom->begin, atom->begin + pos_in_atom,
                         atom->content.substr(0, pos_in_atom),
                         atom->fg_color, atom->bg_color, atom->attribute);

    atom->begin = atom->begin + pos_in_atom;
    atom->content = atom->content.substr(pos_in_atom);
    return insert(atom, std::move(new_atom));
}

void DisplayBuffer::check_invariant() const
{
    for (size_t i = 0; i < m_atoms.size(); ++i)
    {
        assert(m_atoms[i].end > m_atoms[i].begin);
        if (i > 0)
            assert(m_atoms[i-1].end == m_atoms[i].begin);
    }
}

}
