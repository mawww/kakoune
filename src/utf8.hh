#ifndef utf8_hh_INCLUDED
#define utf8_hh_INCLUDED

#include "assert.hh"
#include "unicode.hh"
#include "units.hh"

#include <cstddef>

namespace Kakoune
{

namespace utf8
{

// returns an iterator to next character first byte
template<typename Iterator>
Iterator next(Iterator it, const Iterator& end)
{
    if (it != end and *it++ & 0x80)
        while (it != end and (*(it) & 0xC0) == 0x80)
            ++it;
    return it;
}

// returns it's parameter if it points to a character first byte,
// or else returns next character first byte
template<typename Iterator>
Iterator finish(Iterator it, const Iterator& end)
{
    while (it != end and (*(it) & 0xC0) == 0x80)
        ++it;
    return it;
}

// returns an iterator to the previous character first byte
template<typename Iterator>
Iterator previous(Iterator it, const Iterator& begin)
{
    while (it != begin and (*(--it) & 0xC0) == 0x80)
           ;
    return it;
}

// returns an iterator pointing to the first byte of the
// dth character after (or before if d < 0) the character
// pointed by it
template<typename Iterator>
Iterator advance(Iterator it, const Iterator& end, CharCount d)
{
    if (d < 0)
    {
       while (it != end and d++)
           it = utf8::previous(it, end);
    }
    else
    {
        while (it != end and d--)
           it = utf8::next(it, end);
    }
    return it;
}

// return true if it points to the first byte of a (either single or
// multibyte) character
inline bool is_character_start(char c)
{
    return (c & 0xC0) != 0x80;
}

// returns the character count between begin and end
template<typename Iterator>
CharCount distance(Iterator begin, const Iterator& end)
{
    CharCount dist = 0;

    while (begin != end)
    {
        if (is_character_start(*begin++))
            ++dist;
    }
    return dist;
}

// returns an iterator to the first byte of the character it is into
template<typename Iterator>
Iterator character_start(Iterator it, const Iterator& begin)
{
    while (it != begin and not is_character_start(*it))
        --it;
    return it;
}

namespace InvalidPolicy
{

struct Assert
{
    Codepoint operator()(Codepoint cp) const { kak_assert(false); return cp; }
};

struct Pass
{
    Codepoint operator()(Codepoint cp) const { return cp; }
};

}

// returns the codepoint of the character whose first byte
// is pointed by it
template<typename InvalidPolicy = utf8::InvalidPolicy::Pass,
         typename Iterator>
Codepoint read_codepoint(Iterator& it, const Iterator& end)
{
    if (it == end)
        return InvalidPolicy{}(-1);
    // According to rfc3629, UTF-8 allows only up to 4 bytes.
    // (21 bits codepoint)
    unsigned char byte = *it++;
    if (not (byte & 0x80)) // 0xxxxxxx
        return byte;

    if (it == end)
        return InvalidPolicy{}(byte);

    if ((byte & 0xE0) == 0xC0) // 110xxxxx
        return ((byte & 0x1F) << 6) | (*it++ & 0x3F);

    if ((byte & 0xF0) == 0xE0) // 1110xxxx
    {
        Codepoint cp = ((byte & 0x0F) << 12) | ((*it++ & 0x3F) << 6);
        if (it == end)
            return InvalidPolicy{}(cp);
        return cp | (*it++ & 0x3F);
    }

    if ((byte & 0xF8) == 0xF0) // 11110xxx
    {
        Codepoint cp = ((byte & 0x0F) << 18) | ((*it++ & 0x3F) << 12);
        if (it == end)
            return InvalidPolicy{}(cp);
        cp |= (*it++ & 0x3F) << 6;
        if (it == end)
            return InvalidPolicy{}(cp);
        return cp | (*it++ & 0x3F);
    }
    return InvalidPolicy{}(byte);
}

template<typename InvalidPolicy = utf8::InvalidPolicy::Pass,
         typename Iterator>
Codepoint codepoint(Iterator it, const Iterator& end)
{
    return read_codepoint(it, end);
}

template<typename InvalidPolicy = utf8::InvalidPolicy::Pass>
ByteCount codepoint_size(char byte)
{
    if (not (byte & 0x80)) // 0xxxxxxx
        return 1;
    else if ((byte & 0xE0) == 0xC0) // 110xxxxx
        return 2;
    else if ((byte & 0xF0) == 0xE0) // 1110xxxx
        return 3;
    else if ((byte & 0xF8) == 0xF0) // 11110xxx
        return 4;
    else
    {
        InvalidPolicy{}(byte);
        return 1;
    }
}

struct invalid_codepoint{};

template<typename OutputIterator>
void dump(OutputIterator&& it, Codepoint cp)
{
    if (cp <= 0x7F)
        *it++ = cp;
    else if (cp <= 0x7FF)
    {
        *it++ = 0xC0 | (cp >> 6);
        *it++ = 0x80 | (cp & 0x3F);
    }
    else if (cp <= 0xFFFF)
    {
        *it++ = 0xE0 | (cp >> 12);
        *it++ = 0x80 | ((cp >> 6) & 0x3F);
        *it++ = 0x80 | (cp & 0x3F);
    }
    else if (cp <= 0x10FFFF)
    {
        *it++ = 0xF0 | (cp >> 18);
        *it++ = 0x80 | ((cp >> 12) & 0x3F);
        *it++ = 0x80 | ((cp >> 6)  & 0x3F);
        *it++ = 0x80 | (cp & 0x3F);
    }
    else
        throw invalid_codepoint{};
}

}

}

#endif // utf8_hh_INCLUDED
