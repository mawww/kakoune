#include "face_registry.hh"

#include "exception.hh"

namespace Kakoune
{

static Face parse_face(StringView facedesc)
{
    auto bg_it = std::find(facedesc.begin(), facedesc.end(), ',');
    auto attr_it = std::find(facedesc.begin(), facedesc.end(), '+');
    if (bg_it != facedesc.end() and attr_it < bg_it)
        throw runtime_error("invalid face description, expected <fg>[,<bg>][+<attr>]");
    Face res;
    res.fg = str_to_color({facedesc.begin(), std::min(attr_it, bg_it)});
    if (bg_it != facedesc.end())
        res.bg = str_to_color({bg_it+1, attr_it});
    if (attr_it != facedesc.end())
    {
        for (++attr_it; attr_it != facedesc.end(); ++attr_it)
        {
            switch (*attr_it)
            {
                case 'u': res.attributes |= Underline; break;
                case 'r': res.attributes |= Reverse; break;
                case 'b': res.attributes |= Bold; break;
                default: throw runtime_error("unknown face attribute '" + String(*attr_it) + "'");
            }
        }
    }
    return res;
}

Face FaceRegistry::operator[](const String& facedesc)
{
    auto it = m_aliases.find(facedesc);
    if (it != m_aliases.end())
        return it->second;
    return parse_face(facedesc);
}

void FaceRegistry::register_alias(const String& name, const String& facedesc,
                                   bool override)
{
    if (not override and m_aliases.find(name) != m_aliases.end())
        throw runtime_error("alias '" + name + "' already defined");

    if (name.empty() or
        find_if(name, [](char c){ return not isalnum(c); }) != name.end())
        throw runtime_error("invalid alias name");

    auto it = m_aliases.find(facedesc);
    m_aliases[name] = (it != m_aliases.end()) ?
                      it->second : parse_face(facedesc);
}

CandidateList FaceRegistry::complete_alias_name(StringView prefix,
                                                 ByteCount cursor_pos) const
{
    CandidateList res;
    auto real_prefix = prefix.substr(0, cursor_pos);
    for (auto& alias : m_aliases)
    {
        if (prefix_match(alias.first, real_prefix))
            res.push_back(alias.first);
    }
    return res;
}

FaceRegistry::FaceRegistry()
    : m_aliases{
        { "PrimarySelection", { Colors::Cyan, Colors::Blue } },
        { "SecondarySelection", { Colors::Black, Colors::Blue } },
        { "PrimaryCursor", { Colors::Black, Colors::White } },
        { "SecondaryCursor", { Colors::Black, Colors::White } },
        { "LineNumbers", { Colors::Default, Colors::Default } },
        { "MenuForeground", { Colors::White, Colors::Blue } },
        { "MenuBackground", { Colors::Blue, Colors::White } },
        { "Information", { Colors::Black, Colors::Yellow } },
        { "Error", { Colors::Black, Colors::Red } },
        { "StatusLine", { Colors::Cyan, Colors::Default } },
        { "StatusCursor", { Colors::Black, Colors::Cyan } },
        { "Prompt", { Colors::Yellow, Colors::Default } },
        { "MatchingChar", { Colors::Default, Colors::Default, Underline } },
        { "Search", { Colors::Default, Colors::Default, Underline } },
      }
{}

}
