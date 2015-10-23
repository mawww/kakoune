#ifndef face_hh_INCLUDED
#define face_hh_INCLUDED

#include "color.hh"
#include "flags.hh"

namespace Kakoune
{

enum class Attribute : int
{
    Normal    = 0,
    Exclusive = 1 << 1,
    Underline = 1 << 2,
    Reverse   = 1 << 3,
    Blink     = 1 << 4,
    Bold      = 1 << 5,
    Dim       = 1 << 6,
    Italic    = 1 << 7,
};

template<> struct WithBitOps<Attribute> : std::true_type {};

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

constexpr Face merge_faces(const Face& base, const Face& face)
{
    return face.attributes & Attribute::Exclusive ?
       face : Face{ face.fg == Color::Default ? base.fg : face.fg,
                    face.bg == Color::Default ? base.bg : face.bg,
                    face.attributes | base.attributes };
}

}

#endif // face_hh_INCLUDED
