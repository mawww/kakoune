#include "color_registry.hh"

#include "exception.hh"

namespace Kakoune
{

static Color parse_color(const String& color)
{
    if (color == "default") return Color::Default;
    if (color == "black")   return Color::Black;
    if (color == "red")     return Color::Red;
    if (color == "green")   return Color::Green;
    if (color == "yellow")  return Color::Yellow;
    if (color == "blue")    return Color::Blue;
    if (color == "magenta") return Color::Magenta;
    if (color == "cyan")    return Color::Cyan;
    if (color == "white")   return Color::White;
    throw runtime_error("Unable to parse color '" + color + "'");
    return Color::Default;
}

const ColorPair& ColorRegistry::operator[](const String& colordesc)
{
    auto alias_it = m_aliases.find(colordesc);
    if (alias_it != m_aliases.end())
        return alias_it->second;

    auto it = std::find(colordesc.begin(), colordesc.end(), ',');
    ColorPair colpair{ parse_color(String(colordesc.begin(), it)),
                       it != colordesc.end() ?
                           parse_color(String(it+1, colordesc.end()))
                         : Color::Default };

    m_aliases[colordesc] = colpair;
}

void ColorRegistry::register_alias(const String& name, const String& colordesc,
                                   bool override)
{
    if (not override and m_aliases.find(name) != m_aliases.end())
       throw runtime_error("alias '" + name + "' already defined");

    if (std::find_if(name.begin(), name.end(),
                     [](char c) { return not isalnum(c); }) != name.end())
        throw runtime_error("alias names are limited to alpha numeric words");

    auto it = std::find(colordesc.begin(), colordesc.end(), ',');
    auto fg = parse_color(String(colordesc.begin(), it));
    auto bg = Color::Default;
    if (it != colordesc.end())
        bg = parse_color(String(it+1, colordesc.end()));

    m_aliases[name] = { fg, bg };
}

}
