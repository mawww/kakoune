#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

DisplayLine::iterator DisplayLine::split(iterator it, BufferIterator pos)
{
    assert(it->content.type() == AtomContent::BufferRange);
    assert(it->content.begin() < pos);
    assert(it->content.end() > pos);

    DisplayAtom atom = *it;
    atom.content.m_end = pos;
    it->content.m_begin = pos;
    return m_atoms.insert(it, std::move(atom));
}

void DisplayLine::optimize()
{
    for (auto atom_it = m_atoms.begin(); atom_it != m_atoms.end(); ++atom_it)
    {
        decltype(atom_it) next_atom_it;
        while ((next_atom_it = atom_it + 1) != m_atoms.end())
        {
            auto& atom = *atom_it;
            auto& next_atom = *next_atom_it;

            if (atom.colors == next_atom.colors and
                atom.attribute == next_atom.attribute and
                atom.content.type() == AtomContent::BufferRange and
                next_atom.content.type() == AtomContent::BufferRange and
                next_atom.content.begin() == atom.content.end())
            {
                atom.content.m_end = next_atom.content.end();
                atom_it = m_atoms.erase(next_atom_it) - 1;
            }
            else
                break;
        }
    }
}

CharCount DisplayLine::length() const
{
    CharCount len = 0;
    for (auto& atom : m_atoms)
        len += atom.content.length();
    return len;
}

void DisplayBuffer::compute_range()
{
    m_range.first  = BufferIterator();
    m_range.second = BufferIterator();
    for (auto& line : m_lines)
    {
        for (auto& atom : line)
        {
            if (not atom.content.has_buffer_range())
                continue;

            if (not m_range.first.is_valid() or m_range.first > atom.content.begin())
                m_range.first = atom.content.begin();

            if (not m_range.second.is_valid() or m_range.second < atom.content.end())
                m_range.second = atom.content.end();
        }
    }
    assert(m_range.first.is_valid() and m_range.second.is_valid());
    assert(m_range.first <= m_range.second);
}

void DisplayBuffer::optimize()
{
    for (auto& line : m_lines)
        line.optimize();
}
}
