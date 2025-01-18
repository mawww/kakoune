#ifndef string_utils_hh_INCLUDED
#define string_utils_hh_INCLUDED

#include "string.hh"
#include "enum.hh"
#include "vector.hh"
#include "optional.hh"
#include "format.hh"
#include "array.hh"

namespace Kakoune
{

String trim_indent(StringView str);

String escape(StringView str, StringView characters, char escape);
String unescape(StringView str, StringView characters, char escape);

template<char character, char escape>
String unescape(StringView str)
{
    const char to_escape[2] = { character, escape };
    return unescape(str, {to_escape, 2}, escape);
}

String indent(StringView str, StringView indent = "    ");

String replace(StringView str, StringView substr, StringView replacement);

String left_pad(StringView str, ColumnCount size, Codepoint c = ' ');
String right_pad(StringView str, ColumnCount size, Codepoint c = ' ');

template<typename Container>
String join(const Container& container, char joiner, bool esc_joiner = true)
{
    const char to_escape[2] = { joiner, '\\' };
    String res;
    for (const auto& str : container)
    {
        if (not res.empty())
            res += joiner;
        res += esc_joiner ? escape(str, {to_escape, 2}, '\\') : str;
    }
    return res;
}

template<typename Container>
String join(const Container& container, StringView joiner)
{
    String res;
    for (const auto& str : container)
    {
        if (not res.empty())
            res += joiner;
        res += str;
    }
    return res;
}

inline bool prefix_match(StringView str, StringView prefix)
{
    return str.substr(0_byte, prefix.length()) == prefix;
}

bool subsequence_match(StringView str, StringView subseq);

String expand_tabs(StringView line, ColumnCount tabstop, ColumnCount col = 0);

int str_to_int(StringView str); // throws on error
Optional<int> str_to_int_ifp(StringView str);

String double_up(StringView s, StringView characters);

inline String quote(StringView s)
{
    return format("'{}'", double_up(s, "'"));
}

inline String shell_quote(StringView s)
{
    return format("'{}'", replace(s, "'", R"('\'')"));
}

enum class Quoting
{
    Raw,
    Kakoune,
    Shell
};

constexpr auto enum_desc(Meta::Type<Quoting>)
{
    return make_array<EnumDesc<Quoting>>({
        { Quoting::Raw, "raw" },
        { Quoting::Kakoune, "kakoune" },
        { Quoting::Shell, "shell" }
    });
}

inline auto quoter(Quoting quoting)
{
    switch (quoting)
    {
        case Quoting::Kakoune: return &quote;
        case Quoting::Shell: return &shell_quote;
        case Quoting::Raw:
        default:
            return +[](StringView s) { return s.str(); };
    }
}

inline String option_to_string(StringView opt, Quoting quoting) { return quoter(quoting)(opt); }
inline Vector<String> option_to_strings(StringView opt) { return {opt.str()}; }
inline String option_from_string(Meta::Type<String>, StringView str) { return str.str(); }
inline bool option_add(String& opt, StringView val) { opt += val; return not val.empty(); }

}

#endif // string_utils_hh_INCLUDED
