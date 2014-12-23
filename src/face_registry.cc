#include "face_registry.hh"

#include "containers.hh"
#include "exception.hh"

namespace Kakoune
{

static Face parse_face(StringView facedesc)
{
    auto bg_it = find(facedesc, ',');
    auto attr_it = find(facedesc, '+');
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
                case 'u': res.attributes |= Attribute::Underline; break;
                case 'r': res.attributes |= Attribute::Reverse; break;
                case 'b': res.attributes |= Attribute::Bold; break;
                case 'B': res.attributes |= Attribute::Blink; break;
                case 'd': res.attributes |= Attribute::Dim; break;
                default: throw runtime_error("unknown face attribute '" + String(*attr_it) + "'");
            }
        }
    }
    return res;
}

Face FaceRegistry::operator[](const String& facedesc)
{
    auto it = m_aliases.find(facedesc);
    while (it != m_aliases.end())
    {
        if (it->second.alias.empty())
            return it->second.face;
        it = m_aliases.find(it->second.alias);
    }
    return parse_face(facedesc);
}

void FaceRegistry::register_alias(const String& name, const String& facedesc,
                                  bool override)
{
    if (not override and m_aliases.find(name) != m_aliases.end())
        throw runtime_error("alias '" + name + "' already defined");

    if (name.empty() or is_color_name(name) or
        std::any_of(name.begin(), name.end(),
                    [](char c){ return not isalnum(c); }))
        throw runtime_error("invalid alias name");

    FaceOrAlias& alias = m_aliases[name];
    auto it = m_aliases.find(facedesc);
    if (it != m_aliases.end())
    {
        while (it != m_aliases.end())
        {
            if (it->second.alias.empty())
                break;
            if (it->second.alias == name)
                throw runtime_error("face cycle detected");
            it = m_aliases.find(it->second.alias);
        }

        alias.alias = facedesc;
    }
    else
    {
        alias.alias = "";
        alias.face = parse_face(facedesc);
    }
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
        { "PrimarySelection", Face{ Colors::White, Colors::Blue } },
        { "SecondarySelection", Face{ Colors::Black, Colors::Blue } },
        { "PrimaryCursor", Face{ Colors::Black, Colors::White } },
        { "SecondaryCursor", Face{ Colors::Black, Colors::White } },
        { "LineNumbers", Face{ Colors::Default, Colors::Default } },
        { "MenuForeground", Face{ Colors::White, Colors::Blue } },
        { "MenuBackground", Face{ Colors::Blue, Colors::White } },
        { "Information", Face{ Colors::Black, Colors::Yellow } },
        { "Error", Face{ Colors::Black, Colors::Red } },
        { "StatusLine", Face{ Colors::Cyan, Colors::Default } },
        { "StatusCursor", Face{ Colors::Black, Colors::Cyan } },
        { "Prompt", Face{ Colors::Yellow, Colors::Default } },
        { "MatchingChar", Face{ Colors::Default, Colors::Default, Attribute::Underline } },
        { "Search", Face{ Colors::Default, Colors::Default, Attribute::Underline } },
      }
{}

}
