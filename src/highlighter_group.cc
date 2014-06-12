#include "highlighter_group.hh"

namespace Kakoune
{

void HighlighterGroup::operator()(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer) const
{
    for (auto& hl : m_highlighters)
       hl.second(context, flags, display_buffer);
}

void HighlighterGroup::append(HighlighterAndId&& hl)
{
    if (m_highlighters.contains(hl.first))
        throw runtime_error("duplicate id: " + hl.first);

    m_highlighters.append(std::move(hl));
}
void HighlighterGroup::remove(StringView id)
{
    m_highlighters.remove(id);
}

HighlighterGroup& HighlighterGroup::get_group(StringView path, Codepoint path_separator)
{
    auto sep_it = std::find(path.begin(), path.end(), path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_highlighters.find(id);
    if (it == m_highlighters.end())
        throw group_not_found("no such id: "_str + id);
    if (auto* group = it->second.target<HighlighterGroup>())
    {
        if (sep_it != path.end())
            return group->get_group({sep_it+1, path.end()}, path_separator);
        return *group;
    }
    else if (auto* hier_group = it->second.target<HierachicalHighlighter>())
    {
        if (sep_it == path.end())
            throw group_not_found("not a leaf group: "_str + id);
        return hier_group->get_group({sep_it+1, path.end()}, path_separator);
    }
    else
        throw group_not_found("not a group: "_str + id);
}

HighlighterFunc HighlighterGroup::get_highlighter(StringView path, Codepoint path_separator) const
{
    auto sep_it = std::find(path.begin(), path.end(), path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_highlighters.find(id);
    if (it == m_highlighters.end())
        throw group_not_found("no such id: "_str + id);
    if (sep_it == path.end())
        return HighlighterFunc{std::ref(it->second)};
    else if (auto* group = it->second.target<HighlighterGroup>())
        return group->get_highlighter({sep_it+1, path.end()}, path_separator);
    else if (auto* hier_group = it->second.target<HierachicalHighlighter>())
        return hier_group->get_highlighter({sep_it+1, path.end()}, path_separator);
    else
        throw group_not_found("not a group: "_str + id);
}

CandidateList HighlighterGroup::complete_id(StringView prefix, ByteCount cursor_pos) const
{
    return m_highlighters.complete_id(prefix, cursor_pos);
}

CandidateList HighlighterGroup::complete_group_id(StringView prefix, ByteCount cursor_pos) const
{
    return m_highlighters.complete_id_if(
        prefix, cursor_pos, [](const HighlighterAndId& func) {
            return func.second.template target<HighlighterGroup>() != nullptr;
        });
}

HighlighterGroup& HierachicalHighlighter::get_group(StringView path, Codepoint path_separator)
{
    auto sep_it = std::find(path.begin(), path.end(), path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_groups.find(id);
    if (it == m_groups.end())
        throw group_not_found("no such id: "_str + id);
    if (sep_it != path.end())
        return it->second.get_group(StringView(sep_it+1, path.end()), path_separator);
    else
        return it->second;
}

HighlighterFunc HierachicalHighlighter::get_highlighter(StringView path, Codepoint path_separator) const
{
    auto sep_it = std::find(path.begin(), path.end(), path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_groups.find(id);
    if (it == m_groups.end())
        throw group_not_found("no such id: "_str + id);
    if (sep_it != path.end())
        return it->second.get_highlighter(StringView(sep_it+1, path.end()), path_separator);
    else
        return HighlighterFunc(std::ref(it->second));
}

}
