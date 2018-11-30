#ifndef color_hh_INCLUDED
#define color_hh_INCLUDED

#include "hash.hh"
#include "meta.hh"

namespace Kakoune
{

class String;
class StringView;

struct Color
{
    enum NamedColor : char
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
        BrightBlack,
        BrightRed,
        BrightGreen,
        BrightYellow,
        BrightBlue,
        BrightMagenta,
        BrightCyan,
        BrightWhite,
        RGB,
    };

    NamedColor color;
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;

    constexpr Color() : Color{Default} {}
    constexpr Color(NamedColor c) : color{c} {}
    constexpr Color(unsigned char r, unsigned char g, unsigned char b)
        : color{RGB}, r{r}, g{g}, b{b}
    {}
};

constexpr bool operator==(Color lhs, Color rhs)
{
    return lhs.color == rhs.color and lhs.r == rhs.r and lhs.g == rhs.g
           and lhs.b == rhs.b;
}

constexpr bool operator!=(Color lhs, Color rhs) { return not(lhs == rhs); }

Color str_to_color(StringView color);
String to_string(Color color);

String option_to_string(Color color);
Color option_from_string(Meta::Type<Color>, StringView str);

bool is_color_name(StringView color);

constexpr size_t hash_value(const Color& val)
{
    return val.color == Color::RGB ? hash_values(val.color, val.r, val.g, val.b)
                                   : hash_value(val.color);
}

}

#endif // color_hh_INCLUDED
