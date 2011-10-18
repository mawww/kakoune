#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

BufferString DisplayAtom::content() const
{
    if (m_replacement_text.empty())
        return m_begin.buffer().string(m_begin, m_end);
    else
        return m_replacement_text;
}

template<typename Iterator>
static DisplayCoord advance_coord(const DisplayCoord& pos,
                                  Iterator begin, Iterator end)
{
    DisplayCoord res = pos;
    while (begin != end)
    {
        if (*begin == '\n')
        {
            ++res.line;
            res.column = 0;
        }
        else
            ++res.column;
        ++begin;
    }
    return res;
}

DisplayCoord DisplayAtom::end_coord() const
{
    if (m_replacement_text.empty())
        return advance_coord(m_coord, m_begin, m_end);
    else
        return advance_coord(m_coord, m_replacement_text.begin(),
                             m_replacement_text.end());
}

BufferIterator DisplayAtom::iterator_at(const DisplayCoord& coord) const
{
    if (not m_replacement_text.empty() or coord <= m_coord)
        return m_begin;

    DisplayCoord pos = m_coord;
    for (BufferIterator it = m_begin; it != m_end; ++it)
    {
        if (*it == '\n')
        {
            ++pos.line;
            pos.column = 0;
        }
        else
            ++pos.column;

        if (coord == pos)
            return it+1;
        else if (coord < pos)
            return it;
    }
    return m_end;
}

DisplayCoord DisplayAtom::line_and_column_at(const BufferIterator& iterator) const
{
    assert(iterator >= m_begin and iterator < m_end);

    if (not m_replacement_text.empty())
        return m_coord;

    return advance_coord(m_coord, m_begin, iterator);
}

DisplayBuffer::DisplayBuffer()
{
}

DisplayBuffer::iterator DisplayBuffer::insert(iterator where, const DisplayAtom& atom)
{
    iterator res = m_atoms.insert(where, atom);
    check_invariant();
    return res;
}

DisplayBuffer::iterator DisplayBuffer::atom_containing(const BufferIterator& where)
{
    for (iterator it = m_atoms.begin(); it != m_atoms.end(); ++it)
    {
        if (it->end() > where)
            return it;
    }
    return end();
}

DisplayBuffer::iterator DisplayBuffer::split(iterator atom, const BufferIterator& pos)
{
    assert(atom < end());
    assert(pos > atom->begin());
    assert(pos < atom->end());
    DisplayAtom new_atom(atom->coord(), atom->begin(), pos,
                         atom->fg_color(), atom->bg_color(), atom->attribute());

    atom->m_begin = pos;
    atom->m_coord = new_atom.end_coord();
    iterator res = m_atoms.insert(atom, std::move(new_atom));
    check_invariant();
    return res;
}

void DisplayBuffer::check_invariant() const
{
    for (size_t i = 0; i < m_atoms.size(); ++i)
    {
        assert(m_atoms[i].end() >= m_atoms[i].begin());
        if (i > 0)
        {
            assert(m_atoms[i-1].end() == m_atoms[i].begin());
            assert(m_atoms[i-1].end_coord() == m_atoms[i].coord());
        }
    }
}

void DisplayBuffer::replace_atom_content(iterator atom,
                                         const BufferString& replacement)
{
    assert(atom < end());
    atom->m_replacement_text = replacement;

    // update coordinates of subsequents atoms
    DisplayCoord new_coord = atom->end_coord();
    while (true)
    {
        new_coord = atom->end_coord();
        ++atom;

        if (atom == end() or new_coord == atom->m_coord)
            break;
        atom->m_coord = new_coord;
    }
}

}
