#ifndef color_registry_hh_INCLUDED
#define color_registry_hh_INCLUDED

#include <unordered_map>

#include "color.hh"
#include "utils.hh"

namespace Kakoune
{

using ColorPair = std::pair<Color, Color>;

class ColorRegistry : public Singleton<ColorRegistry>
{
public:
    const ColorPair& operator[](const String& colordesc);
    void register_alias(const String& name, const String& colordesc);

private:
    std::unordered_map<String, ColorPair>    m_aliases;
};

}

#endif // color_registry_hh_INCLUDED

