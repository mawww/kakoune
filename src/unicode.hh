#ifndef unicode_hh_INCLUDED
#define unicode_hh_INCLUDED

#include <cstdint>
#include <ctype.h>
#include <wctype.h>

namespace Kakoune
{

using Codepoint = uint32_t;

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

enum WordType { Word, WORD };

template<WordType word_type = Word>
inline bool is_word(Codepoint c)
{
    return c == '_' or iswalnum(c);
}

template<>
inline bool is_word<WORD>(Codepoint c)
{
    return not is_blank(c) and not is_eol(c);
}

inline bool is_punctuation(Codepoint c)
{
    return not (is_word(c) or is_blank(c) or is_eol(c));
}

enum class CharCategories
{
    Blank,
    EndOfLine,
    Word,
    Punctuation,
};

template<WordType word_type = Word>
inline CharCategories categorize(Codepoint c)
{
    if (is_word(c))
        return CharCategories::Word;
    if (is_eol(c))
        return CharCategories::EndOfLine;
    if (is_blank(c))
        return CharCategories::Blank;
    return word_type == WORD ? CharCategories::Word
                             : CharCategories::Punctuation;
}

}

#endif // unicode_hh_INCLUDED
