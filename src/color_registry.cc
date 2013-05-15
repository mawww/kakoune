#include "color_registry.hh"

#include "exception.hh"

namespace Kakoune
{

static ColorPair parse_color_pair(const String& colordesc)
{
    auto it = std::find(colordesc.begin(), colordesc.end(), ',');
    return { str_to_color({colordesc.begin(), it}),
             it != colordesc.end() ? str_to_color({it+1, colordesc.end()})
                                   : Colors::Default };
}

const ColorPair& ColorRegistry::operator[](const String& colordesc)
{
    auto it = m_aliases.find(colordesc);
    if (it != m_aliases.end())
        return it->second;
    return (m_aliases[colordesc] = parse_color_pair(colordesc));
}

void ColorRegistry::register_alias(const String& name, const String& colordesc,
                                   bool override)
{
    if (not override and m_aliases.find(name) != m_aliases.end())
       throw runtime_error("alias '" + name + "' already defined");

    if (name.empty() or
        find_if(name, [](char c){ return not isalnum(c); }) != name.end())
        throw runtime_error("invalid alias name");

    auto it = m_aliases.find(colordesc);
    m_aliases[name] = (it != m_aliases.end()) ?
                      it->second : parse_color_pair(colordesc);
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
