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
    return { face.fg == Color::Default ? base.fg : face.fg,
             face.bg == Color::Default ? base.bg : face.bg,
             face.attributes | base.attributes };
}

}

#endif // face_hh_INCLUDED
