#ifndef unicode_hh_INCLUDED
#define unicode_hh_INCLUDED

#include <cstdint>
#include <ctype.h>

namespace Kakoune
{

using Codepoint = uint32_t;

inline bool is_word(Codepoint c)
{
    return c == '_' or isalnum(c);
}

inline bool is_eol(Codepoint c)
{
    return c == '\n';
}

inline bool is_blank(Codepoint c)
{
    return c == ' ' or c == '\t';
}

inline bool is_horizontal_blank(Codepoint c)
{
    return c == ' ' or c == '\t';
}

}

#endif // unicode_hh_INCLUDED
