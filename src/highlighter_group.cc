#include "highlighter_group.hh"

#include "exception.hh"
#include "utils.hh"

namespace Kakoune
{

void HighlighterGroup::operator()(DisplayBuffer& display_buffer)
{
    for (auto& highlighter : m_highlighters)
       highlighter.second(display_buffer);
}

void HighlighterGroup::append(HighlighterAndId&& highlighter)
{
    if (m_highlighters.contains(highlighter.first))
        throw runtime_error("highlighter id not found " + highlighter.first);

    m_highlighters.append(std::forward<HighlighterAndId>(highlighter));
}

void HighlighterGroup::remove(const std::string& id)
{
    m_highlighters.remove(id);
}

HighlighterGroup& HighlighterGroup::get_group(const std::string& id)
{
    auto it = m_highlighters.find(id);
    if (it == m_highlighters.end())
        throw runtime_error("no such id: " + id);
    HighlighterGroup* group = it->second.target<HighlighterGroup>();
    if (not group)
        throw runtime_error("not a group: " + id);

    return *group;
}


CandidateList HighlighterGroup::complete_id(const std::string& prefix,
                                            size_t cursor_pos)
{
    return m_highlighters.complete_id<str_to_str>(prefix, cursor_pos);
}

CandidateList HighlighterGroup::complete_group_id(const std::string& prefix,
                                                  size_t cursor_pos)
{
    return m_highlighters.complete_id_if<str_to_str>(
        prefix, cursor_pos,
        [](std::pair<std::string, HighlighterFunc>& func)
        { return func.second.target<HighlighterGroup>() != nullptr; });
}

}
