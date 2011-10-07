#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

DisplayBuffer::DisplayBuffer()
{
}

DisplayBuffer::iterator DisplayBuffer::split(iterator atom, const BufferIterator& pos)
{
    assert(atom < end());
    assert(pos > atom->begin);
    assert(pos < atom->end);
    DisplayAtom new_atom(atom->begin, pos,
                         atom->fg_color, atom->bg_color, atom->attribute);

    atom->begin = pos;
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
