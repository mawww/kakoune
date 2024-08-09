#ifndef unicode_hh_INCLUDED
#define unicode_hh_INCLUDED

#include <cwctype>
#include <cwchar>

#include "array_view.hh"
#include "units.hh"

namespace Kakoune
{

using Codepoint = char32_t;

inline bool is_eol(Codepoint c) noexcept
{
    return c == '\n';
}

inline bool is_horizontal_blank(Codepoint c) noexcept
{
    // Characters considered whitespace by ECMA Regex Spec
    //  minus vertical tab
    // <https://262.ecma-international.org/11.0/#sec-white-space>
    return c == '\t'      or
           c == '\f'      or
           c == ' '       or
           c == U'\u00A0' or
           c == U'\uFEFF' or
           c == U'\u1680' or
           c == U'\u2000' or
           c == U'\u2001' or
           c == U'\u2002' or
           c == U'\u2003' or
           c == U'\u2004' or
           c == U'\u2005' or
           c == U'\u2006' or
           c == U'\u2007' or
           c == U'\u2008' or
           c == U'\u2009' or
           c == U'\u200A' or
           c == U'\u2028' or
           c == U'\u2029' or
           c == U'\u202F' or
           c == U'\u205F' or
           c == U'\u3000' ;
}

inline bool is_blank(Codepoint c) noexcept
{
    // Characters considered Line Terminators by ECMA Regex Spec
    //  plus vertical tab
    // <https://262.ecma-international.org/11.0/#sec-line-terminators>
    return c == '\n'              or
           c == '\r'              or
           c == '\v'              or
           c == U'\u2028'         or
           c == U'\u2029'         or
           is_horizontal_blank(c) ;
}

inline bool is_basic_alpha(Codepoint c) noexcept
{
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z');
}

inline bool is_basic_digit(Codepoint c) noexcept
{
    return c >= '0' and c <= '9';
}

enum WordType { Word, WORD };

template<WordType word_type = Word>
inline bool is_word(Codepoint c, ConstArrayView<Codepoint> extra_word_chars = {'_'}) noexcept
{
    if (c < 128 ? is_basic_alpha(c) or is_basic_digit(c) : iswalnum((wchar_t)c))
        return true;
    for (auto cp : extra_word_chars)
        if (c == cp)
            return true;
    return false;
}

template<>
inline bool is_word<WORD>(Codepoint c, ConstArrayView<Codepoint>) noexcept
{
    return not is_blank(c);
}

inline bool is_punctuation(Codepoint c, ConstArrayView<Codepoint> extra_word_chars = {'_'}) noexcept
{
    return not (is_word(c, extra_word_chars) or is_blank(c));
}

inline bool is_identifier(Codepoint c) noexcept
{
    return is_basic_alpha(c) or is_basic_digit(c) or
           c == '_' or c == '-';
}

inline ColumnCount codepoint_width(Codepoint c) noexcept
{
    if (c == '\n')
        return 1;
    const auto width = wcwidth((wchar_t)c);
    return width >= 0 ? width : 1;
}

enum class CharCategories
{
    Blank,
    EndOfLine,
    Word,
    Punctuation,
};

template<WordType word_type = Word>
inline CharCategories categorize(Codepoint c, ConstArrayView<Codepoint> extra_word_chars) noexcept
{
    if (is_eol(c))
        return CharCategories::EndOfLine;
    if (is_horizontal_blank(c))
        return CharCategories::Blank;
    if (word_type == WORD or is_word(c, extra_word_chars))
        return CharCategories::Word;
    return CharCategories::Punctuation;
}

inline char to_lower(char c) noexcept { return c >= 'A' and c <= 'Z' ? c - 'A' + 'a' : c; }
inline char to_upper(char c) noexcept { return c >= 'a' and c <= 'z' ? c - 'a' + 'A' : c; }

inline bool is_lower(char c) noexcept { return c >= 'a' and c <= 'z'; }
inline bool is_upper(char c) noexcept { return c >= 'A' and c <= 'Z'; }

inline Codepoint to_lower(Codepoint cp) noexcept { return cp < 128 ? (Codepoint)to_lower((char)cp) : towlower((wchar_t)cp); }
inline Codepoint to_upper(Codepoint cp) noexcept { return cp < 128 ? (Codepoint)to_upper((char)cp) : towupper((wchar_t)cp); }

inline bool is_lower(Codepoint cp) noexcept { return cp < 128 ? is_lower((char)cp) : iswlower((wchar_t)cp); }
inline bool is_upper(Codepoint cp) noexcept { return cp < 128 ? is_upper((char)cp) : iswupper((wchar_t)cp); }

}

#endif // unicode_hh_INCLUDED
