#ifndef filter_registry_h_INCLUDED
#define filter_registry_h_INCLUDED

#include <string>
#include <unordered_map>

#include "filter.hh"
#include "utils.hh"
#include "completion.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

class Window;

typedef std::vector<std::string> FilterParameters;

typedef std::function<FilterAndId (Buffer& buffer,
                                   const FilterParameters& params)> FilterFactory;

class FilterRegistry : public Singleton<FilterRegistry>
{
public:
    void register_factory(const std::string& name,
                          const FilterFactory& factory);

    void add_filter_to_buffer(Buffer& window,
                              const std::string& factory_name,
                              const FilterParameters& parameters);

    CandidateList complete_filter(const std::string& prefix,
                                  size_t cursor_pos);

private:
    idvaluemap<std::string, FilterFactory> m_factories;
};

}

#endif // filter_registry_h_INCLUDED

