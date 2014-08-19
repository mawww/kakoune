#ifndef color_hh_INCLUDED
#define color_hh_INCLUDED

#include <utility>

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

    bool operator==(Color c) const
    { return color == c.color and r == c.r and g == c.g and b == c.b; }
    bool operator!=(Color c) const
    { return color != c.color or r != c.r or g != c.g or b != c.b; }
};

Color str_to_color(StringView color);
String color_to_str(Color color);

String option_to_string(Color color);
void option_from_string(StringView str, Color& color);

bool is_color_name(StringView color);

}

#endif // color_hh_INCLUDED
