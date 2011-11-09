#include "filter_registry.hh"

#include "exception.hh"
#include "window.hh"

namespace Kakoune
{

struct factory_not_found : public runtime_error
{
    factory_not_found() : runtime_error("factory not found") {}
};

void FilterRegistry::register_factory(const std::string& name,
                                      const FilterFactory& factory)
{
    assert(m_factories.find(name) == m_factories.end());
    m_factories[name] = factory;
}

void FilterRegistry::add_filter_to_window(Window& window,
                                          const std::string& name,
                                          const FilterParameters& parameters)
{
    auto it = m_factories.find(name);
    if (it == m_factories.end())
        throw factory_not_found();

    window.add_filter(it->second(window, parameters));
}

}
