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
                         : Colors::Default };

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
    auto bg = Color{Colors::Default};
    if (it != colordesc.end())
        bg = str_to_color(String(it+1, colordesc.end()));

    m_aliases[name] = { fg, bg };
}

ColorRegistry::ColorRegistry()
    : m_aliases{
        { "PrimarySelection", { Colors::Cyan, Colors::Blue } },
        { "SecondarySelection", { Colors::Black, Colors::Blue } },
        { "PrimaryCursor", { Colors::Black, Colors::White } },
        { "SecondaryCursor", { Colors::Black, Colors::White } },
        { "LineNumbers", { Colors::Black, Colors::White } },
        { "MenuForeground", { Colors::Blue, Colors::Cyan } },
        { "MenuBackground", { Colors::Cyan, Colors::Blue } },
        { "Information", { Colors::Black, Colors::Yellow } },
        { "Error", { Colors::Black, Colors::Red } },
        { "StatusLine", { Colors::Cyan, Colors::Default } },
        { "StatusCursor", { Colors::Black, Colors::Cyan } },
        { "Prompt", { Colors::Yellow, Colors::Default} },
      }
{}

}
