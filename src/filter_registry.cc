#include "filter_registry.hh"

#include "exception.hh"
#include "window.hh"

namespace Kakoune
{

struct factory_not_found : public runtime_error
{
    factory_not_found(const std::string& name)
        : runtime_error("filter factory not found '" + name + "'") {}
};

void FilterRegistry::register_factory(const std::string& name,
                                      const FilterFactory& factory)
{
    assert(not m_factories.contains(name));
    m_factories.append(std::make_pair(name, factory));
}

void FilterRegistry::add_filter_to_window(Window& window,
                                          const std::string& name,
                                          const FilterParameters& parameters)
{
    auto it = m_factories.find(name);
    if (it == m_factories.end())
        throw factory_not_found(name);

    window.add_filter(it->second(window, parameters));
}

CandidateList FilterRegistry::complete_filter(const std::string& prefix,
                                              size_t cursor_pos)
{
    return m_factories.complete_id<str_to_str>(prefix, cursor_pos);
}

}
