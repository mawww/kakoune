#include "display_buffer.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_utils.hh"
#include "utf8.hh"

#include "face_registry.hh"

namespace Kakoune
{

String option_to_string(BufferRange range)
{
    return format("{}.{},{}.{}",
                  range.begin.line+1, range.begin.column+1,
                  range.end.line+1, range.end.column);
}

void option_from_string(StringView str, BufferRange& opt)
{
    auto sep = find_if(str, [](char c){ return c == ',' or c == '+'; });
    auto dot_beg = find(StringView{str.begin(), sep}, '.');
    auto dot_end = find(StringView{sep, str.end()}, '.');

    if (sep == str.end() or dot_beg == sep or
        (*sep == ',' and dot_end == str.end()))
        throw runtime_error(format("'{}' does not follow <line>.<column>,<line>.<column> or <line>.<column>+<len> format", str));

    const BufferCoord beg{str_to_int({str.begin(), dot_beg}) - 1,
                          str_to_int({dot_beg+1, sep}) - 1};

    const bool len = (*sep == '+');
    const BufferCoord end{len ? beg.line : str_to_int({sep+1, dot_end}) - 1,
                          len ? beg.column + str_to_int({sep+1, str.end()})
                              : str_to_int({dot_end+1, str.end()}) };

    if (beg.line < 0 or beg.column < 0 or end.line < 0 or end.column < 0)
        throw runtime_error("coordinates elements should be >= 1");

    opt = { beg, end };
}

StringView DisplayAtom::content() const
{
    switch (m_type)
    {
        case Range:
        {
            auto line = (*m_buffer)[m_range.begin.line];
            if (m_range.begin.line == m_range.end.line)
                return line.substr(m_range.begin.column, m_range.end.column - m_range.begin.column);
            else if (m_range.begin.line+1 == m_range.end.line and m_range.end.column == 0)
                return line.substr(m_range.begin.column);
            break;
        }
        case Text:
        case ReplacedRange:
            return m_text;
    }
    kak_assert(false);
    return {};
}

ColumnCount DisplayAtom::length() const
{
    switch (m_type)
    {
        case Range:
            return column_length(*m_buffer, m_range.begin, m_range.end);
        case Text:
        case ReplacedRange:
            return m_text.column_length();
    }
    kak_assert(false);
    return 0;
}

void DisplayAtom::trim_begin(ColumnCount count)
{
    if (m_type == Range)
        m_range.begin = utf8::advance(m_buffer->iterator_at(m_range.begin),
                                      m_buffer->iterator_at(m_range.end),
                                      count).coord();
    else
        m_text = m_text.substr(count).str();
    check_invariant();
}

void DisplayAtom::trim_end(ColumnCount count)
{
    if (m_type == Range)
        m_range.end = utf8::advance(m_buffer->iterator_at(m_range.end),
                                    m_buffer->iterator_at(m_range.begin),
                                    -count).coord();
    else
        m_text = m_text.substr(0, m_text.column_length() - count).str();
    check_invariant();
}

void DisplayAtom::check_invariant() const
{
#ifdef KAK_DEBUG
    if (has_buffer_range())
    {
        kak_assert(m_buffer->is_valid(m_range.begin));
        kak_assert(m_buffer->is_valid(m_range.end));
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
    kak_assert(it->type() == DisplayAtom::Range);
    kak_assert(it->begin() < pos);
    kak_assert(it->end() > pos);

    DisplayAtom atom = *it;
    atom.m_range.end = pos;
    it->m_range.begin = pos;
    atom.check_invariant();
    it->check_invariant();
    return m_atoms.insert(it, std::move(atom));
}

DisplayLine::iterator DisplayLine::split(iterator it, ColumnCount pos)
{
    kak_assert(it->type() == DisplayAtom::Text);
    kak_assert(pos > 0);
    kak_assert(pos < it->length());

    DisplayAtom atom(it->m_text.substr(0, pos).str());
    it->m_text = it->m_text.substr(pos).str();
    atom.check_invariant();
    it->check_invariant();
    return m_atoms.insert(it, std::move(atom));
}

DisplayLine::iterator DisplayLine::insert(iterator it, DisplayAtom atom)
{
    if (atom.has_buffer_range())
    {
        m_range.begin  = std::min(m_range.begin, atom.begin());
        m_range.end = std::max(m_range.end, atom.end());
    }
    return m_atoms.insert(it, std::move(atom));
}

void DisplayLine::push_back(DisplayAtom atom)
{
    if (atom.has_buffer_range())
    {
        m_range.begin  = std::min(m_range.begin, atom.begin());
        m_range.end = std::max(m_range.end, atom.end());
    }
    m_atoms.push_back(std::move(atom));
}

DisplayLine::iterator DisplayLine::erase(iterator beg, iterator end)
{
    auto res = m_atoms.erase(beg, end);
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

        if (atom.face == next_atom.face and
            atom.type() ==  next_atom.type())
        {
            auto type = atom.type();
            if ((type == DisplayAtom::Range or
                 type == DisplayAtom::ReplacedRange) and
                next_atom.begin() == atom.end())
            {
                atom.m_range.end = next_atom.end();
                if (type == DisplayAtom::ReplacedRange)
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

ColumnCount DisplayLine::length() const
{
    ColumnCount len = 0;
    for (auto& atom : m_atoms)
        len += atom.length();
    return len;
}

void DisplayLine::trim(ColumnCount first_col, ColumnCount col_count, bool only_buffer)
{
    for (auto it = begin(); first_col > 0 and it != end(); )
    {
        if (only_buffer and not it->has_buffer_range())
        {
            ++it;
            continue;
        }

        auto len = it->length();
        if (len <= first_col)
        {
            m_atoms.erase(it);
            first_col -= len;
        }
        else
        {
            it->trim_begin(first_col);
            first_col = 0;
        }
    }
    auto it = begin();
    for (; it != end() and col_count > 0; ++it)
        col_count -= it->length();

    if (col_count < 0)
        (it-1)->trim_end(-col_count);
    m_atoms.erase(it, end());

    compute_range();
}

const BufferRange init_range{ {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN} };

void DisplayLine::compute_range()
{
    m_range = init_range;
    for (auto& atom : m_atoms)
    {
        if (not atom.has_buffer_range())
            continue;
        m_range.begin  = std::min(m_range.begin, atom.begin());
        m_range.end = std::max(m_range.end, atom.end());
    }
    if (m_range == init_range)
        m_range = { { 0, 0 }, { 0, 0 } };
    kak_assert(m_range.begin <= m_range.end);
}

void DisplayBuffer::compute_range()
{
    m_range = init_range;
    for (auto& line : m_lines)
    {
        m_range.begin  = std::min(line.range().begin, m_range.begin);
        m_range.end = std::max(line.range().end, m_range.end);
    }
    if (m_range == init_range)
        m_range = { { 0, 0 }, { 0, 0 } };
    kak_assert(m_range.begin <= m_range.end);
}

void DisplayBuffer::optimize()
{
    for (auto& line : m_lines)
        line.optimize();
}

DisplayLine parse_display_line(StringView line, const HashMap<String, DisplayLine>& builtins)
{
    DisplayLine res;
    bool was_antislash = false;
    auto pos = line.begin();
    String content;
    Face face;
    for (auto it = line.begin(), end = line.end(); it != end; ++it)
    {
        const char c = *it;
        if (c == '{')
        {
            if (was_antislash)
            {
                content += StringView{pos, it};
                content.back() = '{';
                pos = it + 1;
            }
            else
            {
                content += StringView{pos, it};
                res.push_back({std::move(content), face});
                content.clear();
                auto closing = std::find(it+1, end, '}');
                if (closing == end)
                    throw runtime_error("unclosed face definition");
                if (*(it+1) == '{' and closing+1 != end and *(closing+1) == '}')
                {
                    auto builtin_it = builtins.find(StringView{it+2, closing});
                    if (builtin_it == builtins.end())
                        throw runtime_error(format("undefined atom {}", StringView{it+2, closing}));
                    for (auto& atom : builtin_it->value)
                        res.push_back(atom);
                    // closing is now at the first char of "}}", advance it to the second
                    ++closing;
                }
                else
                    face = get_face({it+1, closing});
                it = closing;
                pos = closing + 1;
            }
        }
        if (c == '\n') // line breaks are forbidden, replace with space
        {
            content += StringView{pos, it+1};
            content.back() = ' ';
            pos = it + 1;
        }

        if (c == '\\')
        {
            if (was_antislash)
            {
                content += StringView{pos, it};
                pos = it + 1;
                was_antislash = false;
            }
            else
                was_antislash = true;
        }
        else
            was_antislash = false;
    }
    content += StringView{pos, line.end()};
    if (not content.empty())
        res.push_back({std::move(content), face});
    return res;
}

}
