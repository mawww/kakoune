#ifndef color_hh_INCLUDED
#define color_hh_INCLUDED

#include "exception.hh"
#include "hash.hh"
#include "meta.hh"
#include "assert.hh"

namespace Kakoune
{

class String;
class StringView;

struct Color
{
    enum NamedColor : unsigned char
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

    union
    {
        NamedColor color;
        unsigned char a;
    };
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;

    constexpr bool isRGB() const { return a >= RGB; }

    constexpr Color() : Color{Default} {}
    constexpr Color(NamedColor c) : color{c} {}
    constexpr Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        : a{a}, r{r}, g{g}, b{b}
    {
        validate_alpha();
    }

private:
    constexpr void validate_alpha() {
        static_assert(RGB == 17);
        if (a < RGB)
            throw runtime_error("Colors alpha must be > 16");
    }
};

constexpr bool operator==(Color lhs, Color rhs)
{
    return lhs.color == rhs.color and
           lhs.r == rhs.r and lhs.g == rhs.g and lhs.b == rhs.b;
}

constexpr bool operator!=(Color lhs, Color rhs)
{
    return not (lhs == rhs);
}

Color str_to_color(StringView color);
String to_string(Color color);

String option_to_string(Color color);
Color option_from_string(Meta::Type<Color>, StringView str);

bool is_color_name(StringView color);

constexpr size_t hash_value(const Color& val)
{
    return val.isRGB() ?
        hash_values(val.a, val.r, val.g, val.b)
      : hash_value(val.color);
}

}

#endif // color_hh_INCLUDED
