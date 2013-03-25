#ifndef color_hh_INCLUDED
#define color_hh_INCLUDED

#include <utility>

namespace Kakoune
{

class String;

enum class Color : char
{
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

using ColorPair = std::pair<Color, Color>;

Color str_to_color(const String& color);
String color_to_str(const Color& color);

}

#endif // color_hh_INCLUDED

