#include "face_registry.hh"

#include "exception.hh"
#include "ranges.hh"
#include "string_utils.hh"

namespace Kakoune
{

static FaceRegistry::FaceSpec parse_face(StringView facedesc)
{
    constexpr StringView invalid_face_error = "invalid face description, expected [<fg>][,<bg>[,<underline>]][+<attr>][@base] or just [base]";
    if (all_of(facedesc, [](char c){ return is_word(c); }) and not is_color_name(facedesc))
        return {Face{}, facedesc.str()};

    auto bg_it = find(facedesc, ',');
    auto underline_it = bg_it == facedesc.end() ? bg_it : std::find(bg_it+1, facedesc.end(), ',');
    auto attr_it = find(facedesc, '+');
    auto base_it = find(facedesc, '@');
    if (bg_it != facedesc.end()
        and (attr_it < bg_it or (bg_it + 1) == facedesc.end()))
        throw runtime_error(invalid_face_error.str());
    if (attr_it != facedesc.end()
        and (attr_it + 1) == facedesc.end())
        throw runtime_error(invalid_face_error.str());

    auto colors_end = std::min(attr_it, base_it);
    if (underline_it != facedesc.end()
        and underline_it > colors_end)
        throw runtime_error(invalid_face_error.str());

    auto parse_color = [](StringView spec) {
        return spec.empty() ? Color::Default : str_to_color(spec);
    };

    FaceRegistry::FaceSpec spec;
    auto& face = spec.face;
    face.fg = parse_color({facedesc.begin(), std::min(bg_it, colors_end)});
    if (bg_it != facedesc.end())
    {
        face.bg = parse_color({bg_it+1, std::min(underline_it, colors_end)});
        if (underline_it != facedesc.end())
            face.underline = parse_color({underline_it+1, colors_end});
    }
    if (attr_it != facedesc.end())
    {
        for (++attr_it; attr_it != base_it; ++attr_it)
        {
            switch (*attr_it)
            {
                case 'u': face.attributes |= Attribute::Underline; break;
                case 'c': face.attributes |= Attribute::CurlyUnderline; break;
                case 'r': face.attributes |= Attribute::Reverse; break;
                case 'b': face.attributes |= Attribute::Bold; break;
                case 'B': face.attributes |= Attribute::Blink; break;
                case 'd': face.attributes |= Attribute::Dim; break;
                case 'i': face.attributes |= Attribute::Italic; break;
                case 's': face.attributes |= Attribute::Strikethrough; break;
                case 'f': face.attributes |= Attribute::FinalFg; break;
                case 'g': face.attributes |= Attribute::FinalBg; break;
                case 'a': face.attributes |= Attribute::FinalAttr; break;
                case 'F': face.attributes |= Attribute::Final; break;
                default: throw runtime_error(format("no such face attribute: '{}'", StringView{*attr_it}));
            }
        }
    }
    if (base_it != facedesc.end())
        spec.base = String{base_it+1, facedesc.end()};
    return spec;
}

String to_string(Attribute attributes)
{
    if (attributes == Attribute::Normal)
        return "";

    struct Attr { Attribute attr; StringView name; }
    attrs[] {
        { Attribute::Underline, "u" },
        { Attribute::CurlyUnderline, "c" },
        { Attribute::Reverse, "r" },
        { Attribute::Blink, "B" },
        { Attribute::Bold, "b" },
        { Attribute::Dim, "d" },
        { Attribute::Italic, "i" },
        { Attribute::Strikethrough, "s" },
        { Attribute::Final, "F" },
        { Attribute::FinalFg, "f" },
        { Attribute::FinalBg, "g" },
        { Attribute::FinalAttr, "a" },
    };

    auto filteredAttrs = attrs |
                         filter([&](const Attr& a) {
                             if ((attributes & a.attr) != a.attr)
                                 return false;
                             attributes &= ~a.attr;
                             return true;
                         }) | transform([](const Attr& a) { return a.name; });

    return accumulate(filteredAttrs, "+"_str, std::plus<>{});
}

String to_string(Face face)
{
    return format("{},{},{}{}", face.fg, face.bg, face.underline, face.attributes);
}

Face FaceRegistry::operator[](StringView facedesc) const
{
    return resolve_spec(parse_face(facedesc));
}

Face FaceRegistry::resolve_spec(const FaceSpec& spec) const
{
    if (spec.base.empty())
        return spec.face;

    StringView base = spec.base;
    Face face = spec.face;
    for (auto* reg = this; reg != nullptr; reg = reg->m_parent.get())
    {
        auto it = reg->m_faces.find(intern(base));
        if (it == reg->m_faces.end())
            continue;

        if (it->value.base.empty())
            return merge_faces(it->value.face, face);
        if (it->value.base != it->key->strview())
            return merge_faces(reg->resolve_spec(it->value), face);
        else
        {
            face = merge_faces(it->value.face, face);
            base = it->value.base;
        }
    }
    return face;
}

void FaceRegistry::add_face(StringView name, StringView facedesc, bool override)
{
    StringDataPtr interned = intern(name);

    if (not override and m_faces.find(interned) != m_faces.end())
        throw runtime_error(format("face '{}' already defined", name));

    if (name.empty() or is_color_name(name) or
        any_of(name, [](char c){ return not is_word(c); }))
        throw runtime_error(format("invalid face name: '{}'", name));

    FaceSpec spec = parse_face(facedesc);
    spec.face.name = interned->strview();
    auto it = m_faces.find(intern(spec.base));
    if (spec.base == name and it != m_faces.end())
    {
        it->value.face = merge_faces(it->value.face, spec.face);
        it->value.base = spec.base;
        return;
    }

    while (it != m_faces.end() and not it->value.base.empty())
    {
        if (it->value.base == name)
            throw runtime_error("face cycle detected");
        it = m_faces.find(intern(it->value.base));
    }
    m_faces[interned] = std::move(spec);
}

void FaceRegistry::remove_face(StringView name)
{
    m_faces.remove(intern(name));
}

FaceRegistry::FaceRegistry()
{
    auto add = [&](StringView name, Face face) {
        StringDataPtr interned = intern(name);
        face.name = interned->strview();
        m_faces.insert({ interned, { face }});
    };
    add("Default", Face{ Color::Default, Color::Default,  });
    add("PrimarySelection", Face{ Color::White, Color::Blue,  });
    add("SecondarySelection", Face{ Color::Black, Color::Blue,  });
    add("PrimaryCursor", Face{ Color::Black, Color::White,  });
    add("SecondaryCursor", Face{ Color::Black, Color::White,  });
    add("PrimaryCursorEol", Face{ Color::Black, Color::Cyan,  });
    add("SecondaryCursorEol", Face{ Color::Black, Color::Cyan,  });
    add("LineNumbers", Face{ Color::Default, Color::Default,  });
    add("LineNumberCursor", Face{ Color::Default, Color::Default, Attribute::Reverse,  });
    add("LineNumbersWrapped", Face{ Color::Default, Color::Default, Attribute::Italic,  });
    add("WrapMarker", Face{ Color::Blue, Color::Default,  });
    add("MenuForeground", Face{ Color::White, Color::Blue,  });
    add("MenuBackground", Face{ Color::Blue, Color::White,  });
    add("MenuInfo", Face{ Color::Cyan, Color::Default,  });
    add("Information", Face{ Color::Black, Color::Yellow,  });
    add("Error", Face{ Color::Black, Color::Red,  });
    add("DiagnosticError", Face{ Color::Red, Color::Default,  });
    add("DiagnosticWarning", Face{ Color::Yellow, Color::Default,  });
    add("StatusLine", Face{ Color::Cyan, Color::Default,  });
    add("StatusLineMode", Face{ Color::Yellow, Color::Default,  });
    add("StatusLineInfo", Face{ Color::Blue, Color::Default,  });
    add("StatusLineValue", Face{ Color::Green, Color::Default,  });
    add("StatusCursor", Face{ Color::Black, Color::Cyan,  });
    add("Prompt", Face{ Color::Yellow, Color::Default,  });
    add("MatchingChar", Face{ Color::Default, Color::Default, Attribute::Bold,  });
    add("BufferPadding", Face{ Color::Blue, Color::Default,  });
    add("Whitespace", Face{ Color::Default, Color::Default, Attribute::FinalFg,  });
}

}
