#ifndef color_hh_INCLUDED
#define color_hh_INCLUDED

#include "hash.hh"

namespace Kakoune
{

class String;
class StringView;

enum class Colors : char
{
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    RGB,
};

struct Color
{
    Colors color;
    unsigned char r;
    unsigned char g;
    unsigned char b;

    Color() : Color{Colors::Default} {}
    Color(Colors c) : color{c}, r{0}, g{0}, b{0} {}
    Color(unsigned char r, unsigned char g, unsigned char b)
        : color{Colors::RGB}, r{r}, g{g}, b{b} {}
};

inline bool operator==(Color lhs, Color rhs)
{
    return lhs.color == rhs.color and
           lhs.r == rhs.r and lhs.g == rhs.g and lhs.b == rhs.b;
}

inline bool operator!=(Color lhs, Color rhs)
{
    return not (lhs == rhs);
}

Color str_to_color(StringView color);
String color_to_str(Color color);

String option_to_string(Color color);
void option_from_string(StringView str, Color& color);

bool is_color_name(StringView color);

inline size_t hash_value(const Color& val)
{
    return hash_values(val.color, val.r, val.g, val.b);
}

}

#endif // color_hh_INCLUDED
