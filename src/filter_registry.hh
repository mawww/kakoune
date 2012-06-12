#ifndef filter_registry_h_INCLUDED
#define filter_registry_h_INCLUDED

#include <unordered_map>

#include "string.hh"
#include "filter.hh"
#include "utils.hh"
#include "completion.hh"
#include "memoryview.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

class FilterGroup;

typedef memoryview<String> FilterParameters;

typedef std::function<FilterAndId (const FilterParameters& params)> FilterFactory;

class FilterRegistry : public Singleton<FilterRegistry>
{
public:
    void register_factory(const String& name,
                          const FilterFactory& factory);

    void add_filter_to_group(FilterGroup& group,
                             const String& factory_name,
                             const FilterParameters& parameters);

    CandidateList complete_filter(const String& prefix,
                                  size_t cursor_pos);

private:
    idvaluemap<String, FilterFactory> m_factories;
};

}

#endif // filter_registry_h_INCLUDED

