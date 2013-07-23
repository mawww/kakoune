#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

void AtomContent::trim_begin(CharCount count)
{
    if (m_type == BufferRange)
        m_begin = utf8::advance(m_buffer->iterator_at(m_begin),
                                m_buffer->iterator_at(m_end), count).coord();
    else
        m_text = m_text.substr(count);
}

void AtomContent::trim_end(CharCount count)
{
    if (m_type == BufferRange)
        m_end = utf8::advance(m_buffer->iterator_at(m_end),
                              m_buffer->iterator_at(m_begin), -count).coord();
    else
        m_text = m_text.substr(0, m_text.char_length() - count);
}

DisplayLine::DisplayLine(AtomList atoms)
    : m_atoms(std::move(atoms))
{
    compute_range();
}

DisplayLine::iterator DisplayLine::split(iterator it, BufferCoord pos)
{
    kak_assert(it->content.type() == AtomContent::BufferRange);
    kak_assert(it->content.begin() < pos);
    kak_assert(it->content.end() > pos);

    DisplayAtom atom = *it;
    atom.content.m_end = pos;
    it->content.m_begin = pos;
    return m_atoms.insert(it, std::move(atom));
}

DisplayLine::iterator DisplayLine::insert(iterator it, DisplayAtom atom)
{
    if (atom.content.has_buffer_range())
    {
        m_range.first  = std::min(m_range.first, atom.content.begin());
        m_range.second = std::max(m_range.second, atom.content.end());
    }
    return m_atoms.insert(it, std::move(atom));
}

void DisplayLine::push_back(DisplayAtom atom)
{
    if (atom.content.has_buffer_range())
    {
        m_range.first  = std::min(m_range.first, atom.content.begin());
        m_range.second = std::max(m_range.second, atom.content.end());
    }
    m_atoms.push_back(std::move(atom));
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

void DisplayLine::trim(CharCount first_char, CharCount char_count)
{
    for (auto it = begin(); first_char > 0 and it != end(); )
    {
        if (not it->content.has_buffer_range())
        {
            ++it;
            continue;
        }

        auto len = it->content.length();
        if (len <= first_char)
        {
            m_atoms.erase(it);
            first_char -= len;
        }
        else
        {
            it->content.trim_begin(first_char);
            first_char = 0;
        }
    }
    auto it = begin();
    for (; it != end() and char_count > 0; ++it)
        char_count -= it->content.length();

    if (char_count < 0)
        (it-1)->content.trim_end(-char_count);
    m_atoms.erase(it, end());

    compute_range();
}

void DisplayLine::compute_range()
{
    m_range = { {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN} };
    for (auto& atom : m_atoms)
    {
        if (not atom.content.has_buffer_range())
            continue;
        m_range.first  = std::min(m_range.first, atom.content.begin());
        m_range.second = std::max(m_range.second, atom.content.end());
    }
}

void DisplayBuffer::compute_range()
{
    m_range.first  = {INT_MAX,INT_MAX};
    m_range.second = {0,0};
    for (auto& line : m_lines)
    {
        m_range.first  = std::min(line.range().first, m_range.first);
        m_range.second = std::max(line.range().second, m_range.second);
    }
    kak_assert(m_range.first <= m_range.second);
}

void DisplayBuffer::optimize()
{
    for (auto& line : m_lines)
        line.optimize();
}
}
