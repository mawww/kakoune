#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

DisplayLine::iterator DisplayLine::split(iterator it, BufferIterator pos)
{
    kak_assert(it->content.type() == AtomContent::BufferRange);
    kak_assert(it->content.begin() < pos);
    kak_assert(it->content.end() > pos);

    DisplayAtom atom = *it;
    atom.content.m_end = pos;
    it->content.m_begin = pos;
    return m_atoms.insert(it, std::move(atom));
}

void DisplayLine::optimize()
{
    if (m_atoms.empty())
        return;

    auto atom_it = m_atoms.begin();
    auto next_atom_it = atom_it + 1;
    while (next_atom_it != m_atoms.end())
    {
        auto& atom = *atom_it;
        auto& next_atom = *next_atom_it;
        bool merged = false;

        if (atom.colors == next_atom.colors and
            atom.attribute == next_atom.attribute and
            atom.content.type() ==  next_atom.content.type())
        {
            auto type = atom.content.type();
            if ((type == AtomContent::BufferRange or
                 type == AtomContent::ReplacedBufferRange) and
                next_atom.content.begin() == atom.content.end())
            {
                atom.content.m_end = next_atom.content.end();
                if (type == AtomContent::ReplacedBufferRange)
                    atom.content.m_text += next_atom.content.m_text;
                merged = true;
            }
            if (type == AtomContent::Text)
            {
                atom.content.m_text += next_atom.content.m_text;
                merged = true;
            }
        }
        if (merged)
            next_atom_it = m_atoms.erase(next_atom_it);
        else
            atom_it = next_atom_it++;
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
    kak_assert(m_range.first.is_valid() and m_range.second.is_valid());
    kak_assert(m_range.first <= m_range.second);
}

void DisplayBuffer::optimize()
{
    for (auto& line : m_lines)
        line.optimize();
}
}
