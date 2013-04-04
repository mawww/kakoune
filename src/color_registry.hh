#ifndef color_registry_hh_INCLUDED
#define color_registry_hh_INCLUDED

#include <unordered_map>

#include "color.hh"
#include "utils.hh"

namespace Kakoune
{

class ColorRegistry : public Singleton<ColorRegistry>
{
public:
    ColorRegistry();

    const ColorPair& operator[](const String& colordesc);
    void register_alias(const String& name, const String& colordesc,
                        bool override = false);

private:
    std::unordered_map<String, ColorPair>    m_aliases;
};

inline const ColorPair& get_color(const String& colordesc)
{
    return ColorRegistry::instance()[colordesc];
}

}

#endif // color_registry_hh_INCLUDED

