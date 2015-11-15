#ifndef unicode_hh_INCLUDED
#define unicode_hh_INCLUDED

#include <wctype.h>

namespace Kakoune
{

using Codepoint = char32_t;

inline bool is_eol(Codepoint c)
{
    return c == '\n';
}

inline bool is_horizontal_blank(Codepoint c)
{
    return c == ' ' or c == '\t';
}

inline bool is_blank(Codepoint c)
{
    return c == ' ' or c == '\t' or c == '\n';
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
    return not is_horizontal_blank(c) and not is_eol(c);
}

inline bool is_punctuation(Codepoint c)
{
    return not (is_word(c) or is_horizontal_blank(c) or is_eol(c));
}

inline bool is_basic_alpha(Codepoint c)
{
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z');
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
    if (is_horizontal_blank(c))
        return CharCategories::Blank;
    return word_type == WORD ? CharCategories::Word
                             : CharCategories::Punctuation;
}

inline Codepoint to_lower(Codepoint cp) { return towlower((wchar_t)cp); }
inline Codepoint to_upper(Codepoint cp) { return towupper((wchar_t)cp); }

inline char to_lower(char c) { return c >= 'A' and c <= 'Z' ? c - 'A' + 'a' : c; }
inline char to_upper(char c) { return c >= 'a' and c <= 'z' ? c - 'a' + 'A' : c; }

}

#endif // unicode_hh_INCLUDED
