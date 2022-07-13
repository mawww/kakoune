#ifndef face_hh_INCLUDED
#define face_hh_INCLUDED

#include "color.hh"
#include "flags.hh"

namespace Kakoune
{

enum class Attribute : int
{
    Normal         = 0,
    Underline      = 1 << 1,
    CurlyUnderline = 1 << 2,
    Reverse        = 1 << 3,
    Blink          = 1 << 4,
    Bold           = 1 << 5,
    Dim            = 1 << 6,
    Italic         = 1 << 7,
    Strikethrough  = 1 << 8,
    FinalFg        = 1 << 9,
    FinalBg        = 1 << 10,
    FinalAttr      = 1 << 11,
    Final          = FinalFg | FinalBg | FinalAttr
};

constexpr bool with_bit_ops(Meta::Type<Attribute>) { return true; }

struct Face
{
    Color fg = Color::Default;
    Color bg = Color::Default;
    Attribute attributes = Attribute::Normal;
    Color underline = Color::Default;

    friend constexpr bool operator==(const Face& lhs, const Face& rhs)
    {
        return lhs.fg == rhs.fg and
               lhs.bg == rhs.bg and
               lhs.underline == rhs.underline and
               lhs.attributes == rhs.attributes;
    }

    friend constexpr bool operator!=(const Face& lhs, const Face& rhs)
    {
        return not (lhs == rhs);
    }

    friend constexpr size_t hash_value(const Face& val)
    {
        return hash_values(val.fg, val.bg, val.underline, val.attributes);
    }
};

inline Face merge_faces(const Face& base, const Face& face)
{
    auto alpha_blend = [](Color base, Color color) {
        auto blend = [&](unsigned char Color::*field) {
            int blended = (base.*field * (255 - color.a) + color.*field * color.a) / 255;
            return static_cast<unsigned char>(blended <= 255 ? blended : 255);
        };
        int alpha = color.a + base.a * (255 - color.a) / 255;
        return Color{blend(&Color::r), blend(&Color::g), blend(&Color::b),
                     static_cast<unsigned char>(alpha <= 255 ? alpha : 255)};
    };

    auto choose = [&](Color Face::*color, Attribute final_attr) {
        if (face.attributes & final_attr)
            return face.*color;
        if (base.attributes & final_attr)
            return base.*color;
        if (face.*color == Color::Default)
            return base.*color;
        if ((base.*color).isRGB() and (face.*color).isRGB() and (face.*color).a != 255)
            return alpha_blend(base.*color, face.*color);
        return face.*color;
    };

    return Face{ choose(&Face::fg, Attribute::FinalFg),
                 choose(&Face::bg, Attribute::FinalBg),
                 face.attributes & Attribute::FinalAttr ? face.attributes :
                 base.attributes & Attribute::FinalAttr ? base.attributes :
                 face.attributes | base.attributes,
                 choose(&Face::underline, Attribute{0}) };
}

}

#endif // face_hh_INCLUDED
