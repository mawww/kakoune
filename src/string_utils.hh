#ifndef string_utils_hh_INCLUDED
#define string_utils_hh_INCLUDED

#include "string.hh"
#include "vector.hh"
#include "optional.hh"

namespace Kakoune
{

StringView trim_whitespaces(StringView str);

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

Vector<StringView> wrap_lines(StringView text, ColumnCount max_width);

int str_to_int(StringView str); // throws on error
Optional<int> str_to_int_ifp(StringView str);

inline String option_to_string(StringView opt) { return opt.str(); }
inline String option_from_string(Meta::Type<String>, StringView str) { return str.str(); }
inline bool option_add(String& opt, StringView val) { opt += val; return not val.empty(); }

template<size_t N>
struct InplaceString
{
    static_assert(N < 256, "InplaceString cannot handle sizes >= 256");

    constexpr operator StringView() const { return {m_data, ByteCount{m_length}}; }
    operator String() const { return {m_data, ByteCount{m_length}}; }

    unsigned char m_length;
    char m_data[N];
};

struct Hex { size_t val; };
constexpr Hex hex(size_t val) { return {val}; }

InplaceString<15> to_string(int val);
InplaceString<15> to_string(unsigned val);
InplaceString<23> to_string(long int val);
InplaceString<23> to_string(unsigned long val);
InplaceString<23> to_string(long long int val);
InplaceString<23> to_string(Hex val);
InplaceString<23> to_string(float val);
InplaceString<7>  to_string(Codepoint c);

template<typename RealType, typename ValueType>
decltype(auto) to_string(const StronglyTypedNumber<RealType, ValueType>& val)
{
    return to_string((ValueType)val);
}

namespace detail
{

template<typename T> constexpr bool is_string = std::is_convertible<T, StringView>::value;

template<typename T, class = std::enable_if_t<not is_string<T>>>
decltype(auto) format_param(const T& val) { return to_string(val); }

template<typename T, class = std::enable_if_t<is_string<T>>>
StringView format_param(const T& val) { return val; }

}

String format(StringView fmt, ArrayView<const StringView> params);

template<typename... Types>
String format(StringView fmt, Types&&... params)
{
    return format(fmt, ArrayView<const StringView>{detail::format_param(std::forward<Types>(params))...});
}

StringView format_to(ArrayView<char> buffer, StringView fmt, ArrayView<const StringView> params);

template<typename... Types>
StringView format_to(ArrayView<char> buffer, StringView fmt, Types&&... params)
{
    return format_to(buffer, fmt, ArrayView<const StringView>{detail::format_param(std::forward<Types>(params))...});
}

String double_up(StringView s, StringView characters);

inline String quote(StringView s)
{
    return format("'{}'", double_up(s, "'"));
}

}

#endif // string_utils_hh_INCLUDED
