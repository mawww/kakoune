#ifndef utf8_hh_INCLUDED
#define utf8_hh_INCLUDED

namespace Kakoune
{

namespace utf8
{

using Codepoint = uint32_t;

// returns an iterator to next character first byte
template<typename Iterator>
Iterator next(Iterator it)
{
    if (*it++ & 0x80)
        while ((*(it) & 0xC0) == 0x80)
            ++it;
    return it;
}

// returns it's parameter if it points to a character first byte,
// or else returns next character first byte
template<typename Iterator>
Iterator finish(Iterator it)
{
    while ((*(it) & 0xC0) == 0x80)
        ++it;
    return it;
}

// returns an iterator to the previous character first byte
template<typename Iterator>
Iterator previous(Iterator it)
{
    while ((*(--it) & 0xC0) == 0x80)
           ;
    return it;
}

// returns an iterator pointing to the first byte of the
// dth character after (or before if d < 0) the character
// pointed by it
template<typename Iterator, typename Distance>
Iterator advance(Iterator it, Distance d)
{
    if (d < 0)
    {
       while (d++)
           it = previous(it);
    }
    else
    {
        while (d--)
           it = next(it);
    }
    return it;
}

// returns the character count between begin and end
template<typename Iterator>
size_t distance(Iterator begin, Iterator end)
{
    size_t dist = 0;
    while (begin != end)
    {
        if ((*begin++ & 0xC0) != 0x80)
            ++dist;
    }
}

// return true if it points to the first byte of a (either single or
// multibyte) character
template<typename Iterator>
bool is_character_start(Iterator it)
{
    return (*it & 0xC0) != 0x80;
}

struct invalid_utf8_sequence{};

// returns the codepoint of the character whose first byte
// is pointed by it
template<typename Iterator>
Codepoint codepoint(Iterator it)
{
    // According to rfc3629, UTF-8 allows only up to 4 bytes.
    // (21 bits codepoint)
    Codepoint cp;
    char byte = *it++;
    if (not (byte & 0x80)) // 0xxxxxxx
        cp = byte;
    else if ((byte & 0xE0) == 0xC0) // 110xxxxx
    {
        cp = ((byte & 0x1F) << 6) | (*it & 0x3F);
    }
    else if ((byte & 0xF0) == 0xE0) // 1110xxxx
    {
        cp = ((byte & 0x0F) << 12) | ((*it++ & 0x3F) << 6);
        cp |= (*it & 0x3F);
    }
    else if ((byte & 0xF8) == 0xF0) // 11110xxx
    {
        cp = ((byte & 0x0F) << 18) | ((*it++ & 0x3F) << 12);
        cp |= (*it++ & 0x3F) << 6;
        cp |= (*it & 0x3F);
    }
    else
        throw invalid_utf8_sequence{};
}

}

}

#endif // utf8_hh_INCLUDED
