#ifndef unicode_hh_INCLUDED
#define unicode_hh_INCLUDED

#include <cstdint>

namespace Kakoune
{

using Codepoint = uint32_t;

inline bool is_word(Codepoint c)
{
    return (c >= '0' and c <= '9') or
           (c >= 'a' and c <= 'z') or
           (c >= 'A' and c <= 'Z') or
           c == '_';
}

inline bool is_eol(Codepoint c)
{
    return c == '\n';
}

inline bool is_blank(Codepoint c)
{
    return c == ' ' or c == '\t';
}

}

#endif // unicode_hh_INCLUDED

