#include "color.hh"

#include "containers.hh"
#include "exception.hh"
#include "regex.hh"

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
};

bool is_color_name(StringView color)
{
    return contains(color_names, color);
}

Color str_to_color(StringView color)
{
    auto it = find_if(color_names, [&](const char* c){ return color == c; });
    if (it != std::end(color_names))
        return static_cast<Colors>(it - color_names);

    static const Regex rgb_regex{"rgb:[0-9a-fA-F]{6}"};
    if (regex_match(color.begin(), color.end(), rgb_regex))
    {
        unsigned l;
        sscanf(color.zstr() + 4, "%x", &l);
        return { (unsigned char)((l >> 16) & 0xFF),
                 (unsigned char)((l >> 8) & 0xFF),
                 (unsigned char)(l & 0xFF) };
    }
    throw runtime_error("Unable to parse color '" + color + "'");
    return Colors::Default;
}

String color_to_str(Color color)
{
    if (color.color == Colors::RGB)
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
    return color_to_str(color);
}

void option_from_string(StringView str, Color& color)
{
    color = str_to_color(str);
}

}
