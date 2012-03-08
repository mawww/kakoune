#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

String DisplayAtom::content() const
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

        if (coord <= pos)
            return it+1;
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
    return atom_containing(where, m_atoms.begin());
}

DisplayBuffer::iterator DisplayBuffer::atom_containing(const BufferIterator& where,
                                                       iterator start)
{
    for (iterator it = start; it != m_atoms.end(); ++it)
    {
        if (it->end() > where)
            return it->begin() <= where ? it : end();
    }
    return end();
}

DisplayBuffer::iterator DisplayBuffer::split(iterator atom, const BufferIterator& pos)
{
    assert(pos > atom->begin());
    assert(pos < atom->end());

    BufferIterator end = atom->m_end;
    atom->m_end = pos;

    DisplayAtom new_atom(atom->end_coord(), pos, end,
                         atom->fg_color(), atom->bg_color(), atom->attribute());

    iterator insert_pos = atom;
    ++insert_pos;
    m_atoms.insert(insert_pos, std::move(new_atom));
    check_invariant();
    return atom;
}

void DisplayBuffer::check_invariant() const
{
    const_iterator prev_it;
    for (const_iterator it = begin(); it != end(); ++it)
    {
        assert(it->end() >= it->begin());
        if (it != begin())
            assert(prev_it->end() == it->begin());
        prev_it = it;
    }
}

void DisplayBuffer::replace_atom_content(iterator atom,
                                         const String& replacement)
{
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
