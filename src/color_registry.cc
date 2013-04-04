#include "color_registry.hh"

#include "exception.hh"

namespace Kakoune
{

const ColorPair& ColorRegistry::operator[](const String& colordesc)
{
    auto alias_it = m_aliases.find(colordesc);
    if (alias_it != m_aliases.end())
        return alias_it->second;

    auto it = std::find(colordesc.begin(), colordesc.end(), ',');
    ColorPair colpair{ str_to_color(String(colordesc.begin(), it)),
                       it != colordesc.end() ?
                           str_to_color(String(it+1, colordesc.end()))
                         : Color::Default };

    return (m_aliases[colordesc] = colpair);
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
    auto fg = str_to_color(String(colordesc.begin(), it));
    auto bg = Color::Default;
    if (it != colordesc.end())
        bg = str_to_color(String(it+1, colordesc.end()));

    m_aliases[name] = { fg, bg };
}

ColorRegistry::ColorRegistry()
    : m_aliases{
        { "PrimarySelection", { Color::Cyan, Color::Blue } },
        { "SecondarySelection", { Color::Black, Color::Blue } },
        { "PrimaryCursor", { Color::Black, Color::White } },
        { "SecondaryCursor", { Color::Black, Color::White } },
        { "LineNumbers", { Color::Black, Color::White } },
        { "MenuForeground", { Color::Blue, Color::Cyan } },
        { "MenuBackground", { Color::Cyan, Color::Blue } },
        { "Information", { Color::Black, Color::Yellow } },
      }
{}

}
