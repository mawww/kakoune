#ifndef filter_registry_h_INCLUDED
#define filter_registry_h_INCLUDED

#include <string>
#include <unordered_map>

#include "filter.hh"
#include "utils.hh"

namespace Kakoune
{

typedef std::vector<std::string> FilterParameters;

typedef std::function<FilterAndId (const FilterParameters& params)> FilterFactory;

class FilterRegistry : public Singleton<FilterRegistry>
{
public:
    void register_factory(const std::string& name,
                          const FilterFactory& factory);

    FilterAndId get_filter(const std::string& factory_name,
                           const FilterParameters& parameters);

private:
    std::unordered_map<std::string, FilterFactory> m_factories;
};

}

#endif // filter_registry_h_INCLUDED
