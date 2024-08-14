#ifndef string_utils_hh_INCLUDED
#define string_utils_hh_INCLUDED

#include "string.hh"
#include "enum.hh"
#include "vector.hh"
#include "optional.hh"
#include "utils.hh"
#include "format.hh"
#include "ranges.hh"

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

struct WrapView
{
    struct Iterator
    {
        using difference_type = ptrdiff_t;
        using value_type = StringView;
        using pointer = StringView*;
        using reference = StringView&;
        using iterator_category = std::forward_iterator_tag;

        Iterator(StringView text, ColumnCount max_width);

        Iterator& operator++();
        Iterator operator++(int) { auto copy = *this; ++(*this); return copy; }

        bool operator==(Iterator other) const { return m_remaining == other.m_remaining and m_current == other.m_current; }
        StringView operator*() { return m_current; }

    private:
        StringView m_current;
        StringView m_remaining;
        ColumnCount m_max_width;
    };

    Iterator begin() const { return {text, max_width}; }
    Iterator end()   const { return {{}, 1}; }

    StringView text;
    ColumnCount max_width;
};

inline auto wrap_at(ColumnCount max_width)
{
    return ViewFactory{[=](StringView text) {
        return WrapView{text, max_width};
    }};
}

int str_to_int(StringView str); // throws on error
Optional<int> str_to_int_ifp(StringView str);

String double_up(StringView s, StringView characters);

}

#endif // string_utils_hh_INCLUDED
