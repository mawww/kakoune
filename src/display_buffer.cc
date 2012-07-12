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
    atom.content.end() = pos;
    it->content.begin() = pos;
    return m_atoms.insert(it, std::move(atom));
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

}
