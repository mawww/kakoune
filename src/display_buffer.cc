#include "display_buffer.hh"

#include "assert.hh"
#include <algorithm>

namespace Kakoune
{

String DisplayAtom::content() const
{
    switch (m_content_mode)
    {
    case BufferText:
        return m_begin.buffer().string(m_begin, m_end);
    case ReplacementText:
        return m_replacement_text;
    }
    assert(false);
    return "";
}

static DisplayCoord advance_coord(const DisplayCoord& pos,
                                  const BufferIterator& begin,
                                  const BufferIterator& end)
{
    if (begin.line() == end.line())
        return DisplayCoord(pos.line, pos.column + end.column() - begin.column());
    else
        return DisplayCoord(pos.line + end.line() - begin.line(), end.column());
}

static DisplayCoord advance_coord(const DisplayCoord& pos,
                                  const String& str)
{
    DisplayCoord res = pos;
    for (auto c : str)
    {
        if (c == '\n')
        {
            ++res.line;
            res.column = 0;
        }
        else
            ++res.column;
    }
    return res;
}

DisplayCoord DisplayAtom::end_coord() const
{
    switch (m_content_mode)
    {
    case BufferText:
        return advance_coord(m_coord, m_begin, m_end);
    case ReplacementText:
        return advance_coord(m_coord, m_replacement_text);
    }
    assert(false);
    return { 0, 0 };
}

BufferIterator DisplayAtom::iterator_at(const DisplayCoord& coord) const
{
    if (m_content_mode != BufferText or coord <= m_coord)
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

    if (m_content_mode != BufferText)
        return m_coord;

    return advance_coord(m_coord, m_begin, iterator);
}

DisplayBuffer::DisplayBuffer()
{
}

DisplayBuffer::iterator DisplayBuffer::insert(iterator where, const DisplayAtom& atom)
{
    iterator res = m_atoms.insert(where, atom);
    // check_invariant();
    return res;
}

DisplayBuffer::iterator DisplayBuffer::atom_containing(const BufferIterator& where)
{
    return atom_containing(where, m_atoms.begin());
}

DisplayBuffer::iterator DisplayBuffer::atom_containing(const BufferIterator& where,
                                                       iterator start)
{
    if (where < start->begin())
        return end();

    return std::upper_bound(start, end(), where,
                            [](const BufferIterator& where, const DisplayAtom& atom)
                            { return where < atom.end(); });
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
    // check_invariant();
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
    atom->m_content_mode = DisplayAtom::ReplacementText;
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
