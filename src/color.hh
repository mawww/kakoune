#ifndef color_hh_INCLUDED
#define color_hh_INCLUDED

namespace Kakoune
{

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

}

#endif // color_hh_INCLUDED

