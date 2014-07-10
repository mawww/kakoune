#include "face_registry.hh"

#include "exception.hh"

namespace Kakoune
{

static Face parse_face(const String& facedesc)
{
    auto it = std::find(facedesc.begin(), facedesc.end(), ',');
    return { str_to_color({facedesc.begin(), it}),
             it != facedesc.end() ? str_to_color({it+1, facedesc.end()})
                                   : Colors::Default };
}

const Face& FaceRegistry::operator[](const String& facedesc)
{
    auto it = m_aliases.find(facedesc);
    if (it != m_aliases.end())
        return it->second;
    return (m_aliases[facedesc] = parse_face(facedesc));
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
        { "MatchingChar", { Colors::Default, Colors::Magenta } },
        { "Search", { Colors::Default, Colors::Magenta } },
      }
{}

}
