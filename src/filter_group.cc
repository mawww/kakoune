#include "filter_group.hh"

#include "exception.hh"
#include "utils.hh"

namespace Kakoune
{

void FilterGroup::operator()(Buffer& buffer, Modification& modification)
{
    for (auto& filter : m_filters)
       filter.second(buffer, modification);
}

void FilterGroup::append(FilterAndId&& filter)
{
    if (m_filters.contains(filter.first))
        throw runtime_error("duplicate filter id: " + filter.first);

    m_filters.append(std::forward<FilterAndId>(filter));
}

void FilterGroup::remove(const String& id)
{
    m_filters.remove(id);
}

FilterGroup& FilterGroup::get_group(const String& id)
{
    auto it = m_filters.find(id);
    if (it == m_filters.end())
        throw runtime_error("no such id: " + id);
    FilterGroup* group = it->second.target<FilterGroup>();
    if (not group)
        throw runtime_error("not a group: " + id);

    return *group;
}


CandidateList FilterGroup::complete_id(const String& prefix,
                                            size_t cursor_pos)
{
    return m_filters.complete_id(prefix, cursor_pos);
}

CandidateList FilterGroup::complete_group_id(const String& prefix,
                                                  size_t cursor_pos)
{
    return m_filters.complete_id_if(
        prefix, cursor_pos,
        [](std::pair<String, FilterFunc>& func)
        { return func.second.target<FilterGroup>() != nullptr; });
}

}
