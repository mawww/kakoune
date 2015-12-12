#include "face_registry.hh"

#include "containers.hh"
#include "exception.hh"
#include "containers.hh"

namespace Kakoune
{

static Face parse_face(StringView facedesc)
{
    constexpr StringView invalid_face_error = "invalid face description, expected <fg>[,<bg>][+<attr>]";
    auto bg_it = find(facedesc, ',');
    auto attr_it = find(facedesc, '+');
    if (bg_it != facedesc.end()
        and (attr_it < bg_it or (bg_it + 1) == facedesc.end()))
        throw runtime_error(invalid_face_error.str());
    if (attr_it != facedesc.end()
        and (attr_it + 1) == facedesc.end())
        throw runtime_error(invalid_face_error.str());
    Face res;
    res.fg = attr_it != facedesc.begin() ?
        str_to_color({facedesc.begin(), std::min(attr_it, bg_it)}) : Color::Default;
    if (bg_it != facedesc.end())
        res.bg = bg_it+1 != attr_it ? str_to_color({bg_it+1, attr_it}) : Color::Default;
    if (attr_it != facedesc.end())
    {
        for (++attr_it; attr_it != facedesc.end(); ++attr_it)
        {
            switch (*attr_it)
            {
                case 'e': res.attributes |= Attribute::Exclusive; break;
                case 'u': res.attributes |= Attribute::Underline; break;
                case 'r': res.attributes |= Attribute::Reverse; break;
                case 'b': res.attributes |= Attribute::Bold; break;
                case 'B': res.attributes |= Attribute::Blink; break;
                case 'd': res.attributes |= Attribute::Dim; break;
                case 'i': res.attributes |= Attribute::Italic; break;
                default: throw runtime_error(format("unknown face attribute '{}'", StringView{*attr_it}));
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
        throw runtime_error(format("alias '{}' already defined", name));

    if (name.empty() or is_color_name(name) or
        std::any_of(name.begin(), name.end(),
                    [](char c){ return not isalnum(c); }))
        throw runtime_error(format("invalid alias name: '{}'", name));

    if (name == facedesc)
        throw runtime_error(format("cannot alias face '{}' to itself", name));

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
    using ValueType = std::pair<String, FaceOrAlias>;
    return complete(prefix, cursor_pos,
                    transformed(m_aliases,
                                [](const ValueType& v){ return v.first; }));
}

FaceRegistry::FaceRegistry()
    : m_aliases{
        { "Default", Face{ Color::Default, Color::Default } },
        { "PrimarySelection", Face{ Color::White, Color::Blue } },
        { "SecondarySelection", Face{ Color::Black, Color::Blue } },
        { "PrimaryCursor", Face{ Color::Black, Color::White } },
        { "SecondaryCursor", Face{ Color::Black, Color::White } },
        { "LineNumbers", Face{ Color::Default, Color::Default } },
        { "LineNumberCursor", Face{ Color::Default, Color::Default, Attribute::Reverse } },
        { "MenuForeground", Face{ Color::White, Color::Blue } },
        { "MenuBackground", Face{ Color::Blue, Color::White } },
        { "MenuInfo", Face{ Color::Cyan, Color::Default } },
        { "Information", Face{ Color::Black, Color::Yellow } },
        { "Error", Face{ Color::Black, Color::Red } },
        { "StatusLine", Face{ Color::Cyan, Color::Default } },
        { "StatusLineMode", Face{ Color::Yellow, Color::Default } },
        { "StatusLineInfo", Face{ Color::Blue, Color::Default } },
        { "StatusLineValue", Face{ Color::Green, Color::Default } },
        { "StatusCursor", Face{ Color::Black, Color::Cyan } },
        { "Prompt", Face{ Color::Yellow, Color::Default } },
        { "MatchingChar", Face{ Color::Default, Color::Default, Attribute::Bold } },
      }
{}

}
