#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

void DisplayAtom::trim_begin(CharCount count)
{
    if (m_type == BufferRange)
        m_begin = utf8::advance(m_buffer->iterator_at(m_begin),
                                m_buffer->iterator_at(m_end), count).coord();
    else
        m_text = m_text.substr(count);
    check_invariant();
}

void DisplayAtom::trim_end(CharCount count)
{
    if (m_type == BufferRange)
        m_end = utf8::advance(m_buffer->iterator_at(m_end),
                              m_buffer->iterator_at(m_begin), -count).coord();
    else
        m_text = m_text.substr(0, m_text.char_length() - count);
    check_invariant();
}

void DisplayAtom::check_invariant() const
{
#ifdef KAK_DEBUG
    if (has_buffer_range())
    {
        kak_assert(m_buffer->is_valid(m_begin));
        kak_assert(m_buffer->is_valid(m_end));
    }
#endif
}

DisplayLine::DisplayLine(AtomList atoms)
    : m_atoms(std::move(atoms))
{
    compute_range();
}

DisplayLine::iterator DisplayLine::split(iterator it, BufferCoord pos)
{
    kak_assert(it->type() == DisplayAtom::BufferRange);
    kak_assert(it->begin() < pos);
    kak_assert(it->end() > pos);

    DisplayAtom atom = *it;
    atom.m_end = pos;
    it->m_begin = pos;
    atom.check_invariant();
    it->check_invariant();
    return m_atoms.insert(it, std::move(atom));
}

DisplayLine::iterator DisplayLine::insert(iterator it, DisplayAtom atom)
{
    if (atom.has_buffer_range())
    {
        m_range.first  = std::min(m_range.first, atom.begin());
        m_range.second = std::max(m_range.second, atom.end());
    }
    return m_atoms.insert(it, std::move(atom));
}

void DisplayLine::push_back(DisplayAtom atom)
{
    if (atom.has_buffer_range())
    {
        m_range.first  = std::min(m_range.first, atom.begin());
        m_range.second = std::max(m_range.second, atom.end());
    }
    m_atoms.push_back(std::move(atom));
}

DisplayLine::iterator DisplayLine::erase(iterator beg, iterator end)
{
    iterator res = m_atoms.erase(beg, end);
    compute_range();
    return res;
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
            atom.type() ==  next_atom.type())
        {
            auto type = atom.type();
            if ((type == DisplayAtom::BufferRange or
                 type == DisplayAtom::ReplacedBufferRange) and
                next_atom.begin() == atom.end())
            {
                atom.m_end = next_atom.end();
                if (type == DisplayAtom::ReplacedBufferRange)
                    atom.m_text += next_atom.m_text;
                merged = true;
            }
            if (type == DisplayAtom::Text)
            {
                atom.m_text += next_atom.m_text;
                merged = true;
            }
        }
        if (merged)
            next_atom_it = m_atoms.erase(next_atom_it);
        else
            atom_it = next_atom_it++;
        atom_it->check_invariant();
    }
}

CharCount DisplayLine::length() const
{
    CharCount len = 0;
    for (auto& atom : m_atoms)
        len += atom.length();
    return len;
}

void DisplayLine::trim(CharCount first_char, CharCount char_count)
{
    for (auto it = begin(); first_char > 0 and it != end(); )
    {
        if (not it->has_buffer_range())
        {
            ++it;
            continue;
        }

        auto len = it->length();
        if (len <= first_char)
        {
            m_atoms.erase(it);
            first_char -= len;
        }
        else
        {
            it->trim_begin(first_char);
            first_char = 0;
        }
    }
    auto it = begin();
    for (; it != end() and char_count > 0; ++it)
        char_count -= it->length();

    if (char_count < 0)
        (it-1)->trim_end(-char_count);
    m_atoms.erase(it, end());

    compute_range();
}

constexpr BufferRange init_range{ {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN} };

void DisplayLine::compute_range()
{
    m_range = init_range;
    for (auto& atom : m_atoms)
    {
        if (not atom.has_buffer_range())
            continue;
        m_range.first  = std::min(m_range.first, atom.begin());
        m_range.second = std::max(m_range.second, atom.end());
    }
    if (m_range == init_range)
        m_range = { { 0, 0 }, { 0, 0 } };
    kak_assert(m_range.first <= m_range.second);
}

void DisplayBuffer::compute_range()
{
    m_range = init_range;
    for (auto& line : m_lines)
    {
        m_range.first  = std::min(line.range().first, m_range.first);
        m_range.second = std::max(line.range().second, m_range.second);
    }
    if (m_range == init_range)
        m_range = { { 0, 0 }, { 0, 0 } };
    kak_assert(m_range.first <= m_range.second);
}

void DisplayBuffer::optimize()
{
    for (auto& line : m_lines)
        line.optimize();
}
}
