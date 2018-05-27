#include "color.hh"

#include "exception.hh"
#include "ranges.hh"
#include "string_utils.hh"

#include <cstdio>

namespace Kakoune
{

static constexpr const char* color_names[] = {
    "default",
    "black",
    "red",
    "green",
    "yellow",
    "blue",
    "magenta",
    "cyan",
    "white",
    "bright-black",
    "bright-red",
    "bright-green",
    "bright-yellow",
    "bright-blue",
    "bright-magenta",
    "bright-cyan",
    "bright-white",
};

bool is_color_name(StringView color)
{
    return contains(color_names, color);
}

Color str_to_color(StringView color)
{
    auto it = find_if(color_names, [&](const char* c){ return color == c; });
    if (it != std::end(color_names))
        return static_cast<Color::NamedColor>(it - color_names);

    auto hval = [&color](char c) -> int
    {
        if (c >= 'A' and c <= 'F')
            return 10 + c - 'A';
        else if (c >= 'a' and c <= 'f')
            return 10 + c - 'a';
        else if (c >= '0' and c <= '9')
            return c - '0';
        throw runtime_error(format("invalid digit '{}' in '{}'", c, color));
    };

    if (color.length() == 10 and color.substr(0_byte, 4_byte) == "rgb:")
        return { (unsigned char)(hval(color[4]) * 16 + hval(color[5])),
                 (unsigned char)(hval(color[6]) * 16 + hval(color[7])),
                 (unsigned char)(hval(color[8]) * 16 + hval(color[9])) };

    throw runtime_error(format("unable to parse color: '{}'", color));
    return Color::Default;
}

String to_string(Color color)
{
    if (color.color == Color::RGB)
    {
        char buffer[11];
        sprintf(buffer, "rgb:%02x%02x%02x", color.r, color.g, color.b);
        return buffer;
    }
    else
    {
        size_t index = static_cast<size_t>(color.color);
        kak_assert(index < std::end(color_names) - std::begin(color_names));
        return color_names[index];
    }
}

String option_to_string(Color color)
{
    return to_string(color);
}

Color option_from_string(Meta::Type<Color>, StringView str)
{
    return str_to_color(str);
}

}
