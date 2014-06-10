#include "highlighter_group.hh"

namespace Kakoune
{

void HighlighterGroup::operator()(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer)
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
    HighlighterGroup* group = it->second.template target<HighlighterGroup>();
    if (not group)
        throw group_not_found("not a group: "_str + id);
    if (sep_it != path.end())
        return group->get_group(StringView(sep_it+1, path.end()), path_separator);
    else
        return *group;
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

}
