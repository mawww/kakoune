#include "display_buffer.hh"

#include "assert.hh"
#include "buffer.hh"
#include "face_registry.hh"
#include "utf8.hh"

#include "face_registry.hh"

namespace Kakoune
{

BufferIterator get_iterator(const Buffer& buffer, BufferCoord coord)
{
    // Correct one past the end of line as next line
    if (not buffer.is_end(coord) and coord.column == buffer[coord.line].length())
        coord = coord.line+1;
    return buffer.iterator_at(coord);
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
            return utf8::column_distance(get_iterator(*m_buffer, m_range.begin),
                                         get_iterator(*m_buffer, m_range.end));
        case Text:
        case ReplacedRange:
            return m_text.column_length();
    }
    kak_assert(false);
    return 0;
}

bool DisplayAtom::empty() const
{
    if (m_type == Range)
        return m_range.begin == m_range.end;
    else
        return m_text.empty();
}

ColumnCount DisplayAtom::trim_begin(ColumnCount count)
{
    ColumnCount res = 0;
    if (m_type == Range)
    {
        auto it = get_iterator(*m_buffer, m_range.begin);
        auto end = get_iterator(*m_buffer, m_range.end);
        while (it != end and res < count)
            res += codepoint_width(utf8::read_codepoint(it, end));
        m_range.begin = std::min(it.coord(), m_range.end);
    }
    else
    {
        auto it = m_text.begin();
        while (it != m_text.end() and res < count)
            res += codepoint_width(utf8::read_codepoint(it, m_text.end()));
        m_text = String{it, m_text.end()};
    }

    return res;
}

ColumnCount DisplayAtom::trim_end_to_length(ColumnCount count)
{
    ColumnCount res = 0;
    if (m_type == Range)
    {
        auto it = get_iterator(*m_buffer, m_range.begin);
        auto end = get_iterator(*m_buffer, m_range.end);
        while (it != end and res < count)
            res += codepoint_width(utf8::read_codepoint(it, end));
        m_range.end = std::min(it.coord(), m_range.end);
    }
    else
    {
        auto it = m_text.begin();
        while (it != m_text.end() and res < count)
            res += codepoint_width(utf8::read_codepoint(it, m_text.end()));
        m_text = String{m_text.begin(), it};
    }

    return res;
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
    return m_atoms.insert(it, std::move(atom));
}

DisplayLine::iterator DisplayLine::split(iterator it, ColumnCount count)
{
    kak_assert(count > 0);
    kak_assert(count < it->length());

    if (it->type() == DisplayAtom::Text or it->type() == DisplayAtom::ReplacedRange)
    {
        DisplayAtom atom = *it;
        atom.m_text = atom.m_text.substr(0, count).str();
        it->m_text = it->m_text.substr(count).str();
        return m_atoms.insert(it, std::move(atom));
    }
    auto pos = utf8::advance(get_iterator(it->buffer(), it->begin()),
                             get_iterator(it->buffer(), it->end()),
                             count).coord();
    if (pos == it->begin()) // Can happen if we try to split in the middle of a multi-column codepoint
        return m_atoms.insert(it, {it->buffer(), {pos, pos}, it->face});
    if (pos == it->end())
        return std::prev(m_atoms.insert(std::next(it), {it->buffer(), {pos, pos}, it->face}));
    return split(it, pos);
}

DisplayLine::iterator DisplayLine::split(BufferCoord pos)
{
    auto it = find_if(begin(), end(), [pos](const DisplayAtom& a) {
        return (a.has_buffer_range() && a.begin() >= pos) ||
               (a.type() == DisplayAtom::Range and a.end() > pos);
    });
    if (it == end() or it->begin() >= pos)
        return it;
    return ++split(it, pos);
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

DisplayAtom& DisplayLine::push_back(DisplayAtom atom)
{
    if (atom.has_buffer_range())
    {
        m_range.begin  = std::min(m_range.begin, atom.begin());
        m_range.end = std::max(m_range.end, atom.end());
    }
    m_atoms.push_back(std::move(atom));
    return m_atoms.back();
}

DisplayLine DisplayLine::extract(iterator beg, iterator end)
{
    if (beg == this->begin() and end == this->end())
        return DisplayLine{std::move(*this)};

    DisplayLine extracted{AtomList(std::move_iterator(beg), std::move_iterator(end))};
    m_atoms.erase(beg, end);
    if (extracted.m_range.begin == m_range.begin or
        extracted.m_range.end == m_range.end)
        compute_range();
    return extracted;
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
    for (auto next_it = atom_it + 1; next_it != m_atoms.end(); ++next_it)
    {
        auto& atom = *atom_it;
        auto& next = *next_it;

        const auto type = atom.type();
        if (type == next.type() and atom.face == next.face)
        {
            if (type == DisplayAtom::Text)
                atom.m_text += next.m_text;
            else if ((type == DisplayAtom::Range or
                      type == DisplayAtom::ReplacedRange) and
                     next.begin() == atom.end())
            {
                atom.m_range.end = next.end();
                if (type == DisplayAtom::ReplacedRange)
                    atom.m_text += next.m_text;
            }
            else
                *++atom_it = std::move(*next_it);
        }
        else
            *++atom_it = std::move(*next_it);
    }
    m_atoms.erase(atom_it+1, m_atoms.end());
}

ColumnCount DisplayLine::length() const
{
    ColumnCount len = 0;
    for (auto& atom : m_atoms)
        len += atom.length();
    return len;
}

bool DisplayLine::trim(ColumnCount front, ColumnCount col_count)
{
    return trim_from(0_col, front, col_count);
}

bool DisplayLine::trim_from(ColumnCount first_col, ColumnCount front, ColumnCount col_count)
{
    auto it = begin();
    while (first_col > 0 and it != end())
    {
        auto len = it->length();
        if (len <= first_col)
        {
            ++it;
            first_col -= len;
        }
        else
        {
            it = ++split(it, front);
            first_col = 0;
        }
    }

    auto front_it = it;
    Optional<DisplayAtom> padding;
    while (front > 0 and it != end())
    {
        front -= it->trim_begin(front);
        kak_assert(it->empty() or front <= 0);
        if (front < 0)
            padding.emplace(it->has_buffer_range()
                ? DisplayAtom{it->buffer(), {it->begin(), it->begin()}, String{' ', -front}, it->face}
                : DisplayAtom{String{' ', -front}, it->face});

        if (it->empty())
            ++it;
    }
    it = m_atoms.erase(front_it, it);
    if (padding)
        it = m_atoms.insert(it, std::move(*padding));

    it = begin();
    for (; it != end() and col_count > 0; ++it)
        col_count -= it->trim_end_to_length(col_count);
    bool did_trim = it != end() && col_count == 0;
    m_atoms.erase(it, end());

    compute_range();
    return did_trim;
}

const BufferRange init_range{ {INT_MAX, INT_MAX}, {INT_MIN, INT_MIN} };

void DisplayLine::compute_range()
{
    m_range = init_range;
    auto first = find_if(m_atoms, [](const auto& atom) { return atom.has_buffer_range(); });
    if (first == m_atoms.end())
        m_range = { { 0, 0 }, { 0, 0 } };
    else
    {
        auto last = std::find_if(m_atoms.rbegin(), std::reverse_iterator(first+1), [](const auto& atom) { return atom.has_buffer_range(); });
        m_range.begin = first->begin();
        m_range.end = last->end();
    }

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

DisplayLine parse_display_line(StringView line, Face& face, const FaceRegistry& faces, const HashMap<String, DisplayLine>& builtins)
{
    DisplayLine res;
    bool was_antislash = false;
    auto pos = line.begin();
    String content;
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
                if (not content.empty())
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
                else if (closing == it+2 and *(it+1) == '\\')
                {
                    pos = closing + 1;
                    break;
                }
                else
                    face = faces[{it+1, closing}];
                it = closing;
                pos = closing + 1;
            }
        }
        if (c == '\n' or c == '\t') // line breaks and tabs are forbidden, replace with space
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

DisplayLine parse_display_line(StringView line, const FaceRegistry& faces, const HashMap<String, DisplayLine>& builtins)
{
    Face face{};
    return parse_display_line(line, face, faces, builtins);
}

DisplayLineList parse_display_line_list(StringView content, const FaceRegistry& faces, const HashMap<String, DisplayLine>& builtins)
{
    return content | split<StringView>('\n')
                   | transform([&, face=Face{}](StringView s) mutable {
                         return parse_display_line(s, face, faces, builtins);
                     })
                   | gather<DisplayLineList>();
}

}
