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
    Dim       = 1 << 5
};

template<> struct WithBitOps<Attribute> : std::true_type {};

struct Face
{
    Color fg;
    Color bg;
    Attribute attributes;

    Face(Color fg = Colors::Default, Color bg = Colors::Default,
         Attribute attributes = Attribute::Normal)
      : fg{fg}, bg{bg}, attributes{attributes} {}
};

inline bool operator==(const Face& lhs, const Face& rhs)
{
    return lhs.fg == rhs.fg and
           lhs.bg == rhs.bg and
           lhs.attributes == rhs.attributes;
}

inline bool operator!=(const Face& lhs, const Face& rhs)
{
    return not (lhs == rhs);
}

}

#endif // face_hh_INCLUDED

