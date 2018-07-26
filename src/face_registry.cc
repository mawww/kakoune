#include "face_registry.hh"

#include "exception.hh"
#include "ranges.hh"
#include "string_utils.hh"

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
                default: throw runtime_error(format("no such face attribute: '{}'", StringView{*attr_it}));
            }
        }
    }
    return res;
}

String to_string(Attribute attributes)
{
    if (attributes == Attribute::Normal)
        return "";

    struct Attr { Attribute attr; StringView name; }
    attrs[] {
        { Attribute::Exclusive, "e" },
        { Attribute::Underline, "u" },
        { Attribute::Reverse, "r" },
        { Attribute::Blink, "B" },
        { Attribute::Bold, "b" },
        { Attribute::Dim, "d" },
        { Attribute::Italic, "i" },
    };

    auto filteredAttrs = attrs |
                         filter([=](const Attr& a) { return attributes & a.attr; }) |
                         transform([](const Attr& a) { return a.name; });

    return accumulate(filteredAttrs, "+"_str, std::plus<>{});
}

String to_string(Face face)
{
    return format("{},{}{}", face.fg, face.bg, face.attributes);
}

Face FaceRegistry::operator[](StringView facedesc) const
{
    auto it = m_faces.find(facedesc);
    if (it != m_faces.end())
    {
        if (it->value.alias.empty())
            return it->value.face;
        return operator[](it->value.alias);
    }
    if (m_parent)
        return (*m_parent)[facedesc];
    return parse_face(facedesc);
}

void FaceRegistry::add_face(StringView name, StringView facedesc, bool override)
{
    if (not override and m_faces.find(name) != m_faces.end())
        throw runtime_error(format("face '{}' already defined", name));

    if (name.empty() or is_color_name(name) or
        std::any_of(name.begin(), name.end(),
                    [](char c){ return not is_word(c); }))
        throw runtime_error(format("invalid face name: '{}'", name));

    if (name == facedesc)
        throw runtime_error(format("cannot alias face '{}' to itself", name));

    for (auto it = m_faces.find(facedesc);
         it != m_faces.end() and not it->value.alias.empty();
         it = m_faces.find(it->value.alias))
    {
        if (it->value.alias == name)
            throw runtime_error("face cycle detected");
    }

    FaceOrAlias& face = m_faces[name];

    for (auto* registry = this; registry != nullptr; registry = registry->m_parent.get())
    {
        if (not registry->m_faces.contains(facedesc))
            continue;
        face.alias = facedesc.str(); // This is referencing another face
        return;
    }

    face.alias = "";
    face.face = parse_face(facedesc);
}

void FaceRegistry::remove_face(StringView name)
{
    m_faces.remove(name);
}

FaceRegistry::FaceRegistry()
    : m_faces{
        { "Default", {Face{ Color::Default, Color::Default }} },
        { "PrimarySelection", {Face{ Color::White, Color::Blue }} },
        { "SecondarySelection", {Face{ Color::Black, Color::Blue }} },
        { "PrimaryCursor", {Face{ Color::Black, Color::White }} },
        { "SecondaryCursor", {Face{ Color::Black, Color::White }} },
        { "PrimaryCursorEol", {Face{ Color::Black, Color::Cyan }} },
        { "SecondaryCursorEol", {Face{ Color::Black, Color::Cyan }} },
        { "LineNumbers", {Face{ Color::Default, Color::Default }} },
        { "LineNumberCursor", {Face{ Color::Default, Color::Default, Attribute::Reverse }} },
        { "LineNumbersWrapped", {Face{ Color::Default, Color::Default, Attribute::Italic }} },
        { "MenuForeground", {Face{ Color::White, Color::Blue }} },
        { "MenuBackground", {Face{ Color::Blue, Color::White }} },
        { "MenuInfo", {Face{ Color::Cyan, Color::Default }} },
        { "Information", {Face{ Color::Black, Color::Yellow }} },
        { "Error", {Face{ Color::Black, Color::Red }} },
        { "StatusLine", {Face{ Color::Cyan, Color::Default }} },
        { "StatusLineMode", {Face{ Color::Yellow, Color::Default }} },
        { "StatusLineInfo", {Face{ Color::Blue, Color::Default }} },
        { "StatusLineValue", {Face{ Color::Green, Color::Default }} },
        { "StatusCursor", {Face{ Color::Black, Color::Cyan }} },
        { "Prompt", {Face{ Color::Yellow, Color::Default }} },
        { "MatchingChar", {Face{ Color::Default, Color::Default, Attribute::Bold }} },
        { "BufferPadding", {Face{ Color::Blue, Color::Default }} },
        { "Whitespace", {Face{ Color::Default, Color::Default }} },
      }
{}

}
