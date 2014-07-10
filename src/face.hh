#ifndef face_hh_INCLUDED
#define face_hh_INCLUDED

#include "color.hh"

namespace Kakoune
{

using Attribute = char;
enum Attributes
{
    Normal = 0,
    Underline = 1,
    Reverse = 2,
    Blink = 4,
    Bold = 8
};

struct Face
{
    Color fg;
    Color bg;
    Attribute attributes;

    Face(Color fg = Colors::Default, Color bg = Colors::Default,
         Attribute attributes = 0)
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

