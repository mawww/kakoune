#include "filter_registry.hh"

#include "exception.hh"
#include "buffer.hh"

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

void FilterRegistry::add_filter_to_buffer(Buffer& buffer,
                                          const std::string& name,
                                          const FilterParameters& parameters)
{
    auto it = m_factories.find(name);
    if (it == m_factories.end())
        throw factory_not_found(name);

    buffer.add_filter(it->second(buffer, parameters));
}

CandidateList FilterRegistry::complete_filter(const std::string& prefix,
                                              size_t cursor_pos)
{
    return m_factories.complete_id<str_to_str>(prefix, cursor_pos);
}

}
