#include "highlighter_group.hh"

namespace Kakoune
{

static constexpr Codepoint path_separator = '/';


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

HighlighterGroup& HighlighterGroup::get_group(StringView path)
{
    auto sep_it = find(path, path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_highlighters.find(id);
    if (it == m_highlighters.end())
        throw group_not_found("no such id: "_str + id);
    if (auto* group = it->second.target<HighlighterGroup>())
    {
        if (sep_it != path.end())
            return group->get_group({sep_it+1, path.end()});
        return *group;
    }
    else if (auto* hier_group = it->second.target<HierachicalHighlighter>())
    {
        if (sep_it == path.end())
            throw group_not_found("not a leaf group: "_str + id);
        return hier_group->get_group({sep_it+1, path.end()});
    }
    else
        throw group_not_found("not a group: "_str + id);
}

HighlighterFunc HighlighterGroup::get_highlighter(StringView path) const
{
    auto sep_it = find(path, path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_highlighters.find(id);
    if (it == m_highlighters.end())
        throw group_not_found("no such id: "_str + id);
    if (sep_it == path.end())
        return HighlighterFunc{std::ref(it->second)};
    else if (auto* group = it->second.target<HighlighterGroup>())
        return group->get_highlighter({sep_it+1, path.end()});
    else if (auto* hier_group = it->second.target<HierachicalHighlighter>())
        return hier_group->get_highlighter({sep_it+1, path.end()});
    else
        throw group_not_found("not a group: "_str + id);
}

template<Completions (HighlighterGroup::*hg_complete)(StringView path, ByteCount cursor_pos) const,
         Completions (HierachicalHighlighter::*hh_complete)(StringView path, ByteCount cursor_pos) const,
         typename Condition>
Completions complete_impl(const id_map<HighlighterFunc>& highlighters, Condition condition,
                          StringView path, ByteCount cursor_pos)
{
    auto sep_it = find(path, path_separator);
    StringView id(path.begin(), sep_it);
    if (sep_it == path.end())
        return { 0_byte, path.length(), highlighters.complete_id_if(path, cursor_pos, condition) };

    auto it = highlighters.find(id);
    if (it != highlighters.end())
    {
        const ByteCount offset = (int)(sep_it + 1 - path.begin());
        cursor_pos -= offset;
        if (auto* group = it->second.target<HighlighterGroup>())
            return offset_pos((group->*hg_complete)({sep_it+1, path.end()}, cursor_pos), offset);
        if (auto* hier_group = it->second.target<HierachicalHighlighter>())
            return offset_pos((hier_group->*hh_complete)({sep_it+1, path.end()}, cursor_pos), offset);
    }
    return {};
}

Completions HighlighterGroup::complete_id(StringView path, ByteCount cursor_pos) const
{
    return complete_impl<
        &HighlighterGroup::complete_id,
        &HierachicalHighlighter::complete_id
    >(m_highlighters, [](const HighlighterAndId&) { return true; }, path, cursor_pos);
}

Completions HighlighterGroup::complete_group_id(StringView path, ByteCount cursor_pos) const
{
    return complete_impl<
        &HighlighterGroup::complete_group_id,
        &HierachicalHighlighter::complete_group_id
    >(m_highlighters, [](const HighlighterAndId& func) {
          return func.second.target<HighlighterGroup>() or
                 func.second.target<HierachicalHighlighter>();
      }, path, cursor_pos);
}

HighlighterGroup& HierachicalHighlighter::get_group(StringView path)
{
    auto sep_it = find(path, path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_groups.find(id);
    if (it == m_groups.end())
        throw group_not_found("no such id: "_str + id);
    if (sep_it != path.end())
        return it->second.get_group(StringView(sep_it+1, path.end()));
    else
        return it->second;
}

HighlighterFunc HierachicalHighlighter::get_highlighter(StringView path) const
{
    auto sep_it = find(path, path_separator);
    StringView id(path.begin(), sep_it);
    auto it = m_groups.find(id);
    if (it == m_groups.end())
        throw group_not_found("no such id: "_str + id);
    if (sep_it != path.end())
        return it->second.get_highlighter(StringView(sep_it+1, path.end()));
    else
        return HighlighterFunc(std::ref(it->second));
}

template<Completions (HighlighterGroup::*complete)(StringView path, ByteCount cursor_pos) const>
Completions complete_impl(const HierachicalHighlighter::GroupMap& groups,
                          StringView path, ByteCount cursor_pos)
{
    auto sep_it = find(path, path_separator);
    StringView id(path.begin(), sep_it);
    auto it = groups.find(id);
    if (sep_it == path.end())
        return { 0_byte, id.length(), groups.complete_id(id, cursor_pos) };

    if (it != groups.end())
    {
        const ByteCount offset = (int)(sep_it + 1- path.begin());
        return offset_pos(
            (it->second.*complete)({sep_it+1, path.end()},
                                   cursor_pos - offset), offset);
    }
    return {};
}

Completions HierachicalHighlighter::complete_id(StringView path, ByteCount cursor_pos) const
{
    return complete_impl<&HighlighterGroup::complete_id>(m_groups, path, cursor_pos);
}

Completions HierachicalHighlighter::complete_group_id(StringView path, ByteCount cursor_pos) const
{
    return complete_impl<&HighlighterGroup::complete_group_id>(m_groups, path, cursor_pos);
}

}
