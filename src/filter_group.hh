#ifndef filter_group_hh_INCLUDED
#define filter_group_hh_INCLUDED

#include "filter.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

// FilterGroup is an filter which delegate to multiple
// other filters in order of insertion.
class FilterGroup
{
public:
    void operator()(Buffer& buffer, BufferIterator& position, String& content);

    void append(FilterAndId&& filter);
    void remove(const String& id);

    FilterGroup& get_group(const String& id);

    CandidateList complete_id(const String& prefix, ByteCount cursor_pos);
    CandidateList complete_group_id(const String& prefix, ByteCount cursor_pos);

private:
    idvaluemap<String, FilterFunc> m_filters;
};

}

#endif // filter_group_hh_INCLUDED
