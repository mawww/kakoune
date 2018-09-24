#ifndef face_hh_INCLUDED
#define face_hh_INCLUDED

#include "color.hh"
#include "flags.hh"

namespace Kakoune
{

enum class Attribute : int
{
    Normal    = 0,
    Underline = 1 << 1,
    Reverse   = 1 << 2,
    Blink     = 1 << 3,
    Bold      = 1 << 4,
    Dim       = 1 << 5,
    Italic    = 1 << 6,
    FinalFg   = 1 << 7,
    FinalBg   = 1 << 8,
    FinalAttr = 1 << 9,
    Final     = FinalFg | FinalBg | FinalAttr
};

constexpr bool with_bit_ops(Meta::Type<Attribute>) { return true; }

struct Face
{
    Color fg;
    Color bg;
    Attribute attributes;

    constexpr Face(Color fg = Color::Default, Color bg = Color::Default,
         Attribute attributes = Attribute::Normal)
      : fg{fg}, bg{bg}, attributes{attributes} {}
};

constexpr bool operator==(const Face& lhs, const Face& rhs)
{
    return lhs.fg == rhs.fg and
           lhs.bg == rhs.bg and
           lhs.attributes == rhs.attributes;
}

constexpr bool operator!=(const Face& lhs, const Face& rhs)
{
    return not (lhs == rhs);
}

constexpr size_t hash_value(const Face& val)
{
    return hash_values(val.fg, val.bg, val.attributes);
}

inline Face merge_faces(const Face& base, const Face& face)
{
    auto choose = [&](Color Face::*color, Attribute final_attr) {
        if (face.attributes & final_attr)
            return face.*color;
        if (base.attributes & final_attr)
            return base.*color;
        if (face.*color == Color::Default)
            return base.*color;
        return face.*color;
    };

    return Face{ choose(&Face::fg, Attribute::FinalFg),
                 choose(&Face::bg, Attribute::FinalBg),
                 face.attributes & Attribute::FinalAttr ? face.attributes :
                 base.attributes & Attribute::FinalAttr ? base.attributes :
                 face.attributes | base.attributes };
}

}

#endif // face_hh_INCLUDED
