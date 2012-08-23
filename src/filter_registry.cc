#include "filter_registry.hh"

#include "exception.hh"
#include "filter_group.hh"

namespace Kakoune
{

struct factory_not_found : public runtime_error
{
    factory_not_found(const String& name)
        : runtime_error("filter factory not found '" + name + "'") {}
};

void FilterRegistry::register_factory(const String& name,
                                      const FilterFactory& factory)
{
    assert(not m_factories.contains(name));
    m_factories.append(std::make_pair(name, factory));
}

void FilterRegistry::add_filter_to_group(FilterGroup& group,
                                         const String& name,
                                         const FilterParameters& parameters)
{
    auto it = m_factories.find(name);
    if (it == m_factories.end())
        throw factory_not_found(name);

    group.append(it->second(parameters));
}

CandidateList FilterRegistry::complete_filter(const String& prefix,
                                              CharCount cursor_pos)
{
    return m_factories.complete_id(prefix, cursor_pos);
}

}
