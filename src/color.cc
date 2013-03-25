#include "color.hh"

#include "exception.hh"

namespace Kakoune
{

Color str_to_color(const String& color)
{
    if (color == "default") return Color::Default;
    if (color == "black")   return Color::Black;
    if (color == "red")     return Color::Red;
    if (color == "green")   return Color::Green;
    if (color == "yellow")  return Color::Yellow;
    if (color == "blue")    return Color::Blue;
    if (color == "magenta") return Color::Magenta;
    if (color == "cyan")    return Color::Cyan;
    if (color == "white")   return Color::White;
    throw runtime_error("Unable to parse color '" + color + "'");
    return Color::Default;
}

String color_to_str(const Color& color)
{
    switch (color)
    {
        case Color::Default: return "default";
        case Color::Black:   return "black";
        case Color::Red:     return "red";
        case Color::Green:   return "green";
        case Color::Yellow:  return "yellow";
        case Color::Blue:    return "blue";
        case Color::Magenta: return "magenta";
        case Color::Cyan:    return "cyan";
        case Color::White:   return "white";
    }
    assert(false);
    return "default";
}

}
