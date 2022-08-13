#include "string_utils.hh"

#include "exception.hh"
#include "utf8_iterator.hh"
#include "unit_tests.hh"

#include <charconv>
#include <cstdio>

namespace Kakoune
{

String trim_indent(StringView str)
{
    if (str.empty())
        return {};

    if (str[0_byte] == '\n')
        str = str.substr(1_byte);
    while (not str.empty() and is_blank(str.back()))
        str = str.substr(0, str.length() - 1);

    utf8::iterator it{str.begin(), str};
    while (it != str.end() and is_horizontal_blank(*it))
        ++it;

    const StringView indent{str.begin(), it.base()};
    return accumulate(str | split_after<StringView>('\n') | transform([&](auto&& line) {
            if (line == "\n")
                return line;
            else if (not prefix_match(line, indent))
                throw runtime_error("inconsistent indentation in the string");

            return line.substr(indent.length());
        }), String{}, [](String s, StringView l) { return s += l; });
}

String escape(StringView str, StringView characters, char escape)
{
    String res;
    res.reserve(str.length());
    auto cbeg = characters.begin(), cend = characters.end();
    for (auto it = str.begin(), end = str.end(); it != end; )
    {
        auto next = std::find_first_of(it, end, cbeg, cend);
        if (next != end)
        {
            res += StringView{it, next+1};
            res.back() = escape;
            res += *next;
            it = next+1;
        }
        else
        {
            res += StringView{it, next};
            break;
        }
    }
    return res;
}

String unescape(StringView str, StringView characters, char escape)
{
    String res;
    res.reserve(str.length());
    for (auto it = str.begin(), end = str.end(); it != end; )
    {
        auto next = std::find(it, end, escape);
        if (next != end and next+1 != end and contains(characters, *(next+1)))
        {
            res += StringView{it, next+1};
            res.back() = *(next+1);
            it = next + 2;
        }
        else
        {
            res += StringView{it, next == end ? next : next + 1};
            it = next == end ? next : next + 1;
        }
    }
    return res;
}

String indent(StringView str, StringView indent)
{
    String res;
    res.reserve(str.length());
    bool was_eol = true;
    for (ByteCount i = 0; i < str.length(); ++i)
    {
        if (was_eol)
            res += indent;
        res += str[i];
        was_eol = is_eol(str[i]);
    }
    return res;
}

String replace(StringView str, StringView substr, StringView replacement)
{
    String res;
    for (auto it = str.begin(); it != str.end(); )
    {
        auto match = std::search(it, str.end(), substr.begin(), substr.end());
        res += StringView{it, match};
        if (match == str.end())
            break;

        res += replacement;
        it = match + (int)substr.length();
    }
    return res;
}

String left_pad(StringView str, ColumnCount size, Codepoint c)
{
    return String(c, std::max(0_col, size - str.column_length())) + str.substr(0, size);
}

String right_pad(StringView str, ColumnCount size, Codepoint c)
{
    return str.substr(0, size) + String(c, std::max(0_col, size - str.column_length()));
}

Optional<int> str_to_int_ifp(StringView str)
{
    bool negative = not str.empty() and str[0] == '-';
    if (negative)
        str = str.substr(1_byte);
    if (str.empty())
        return {};

    unsigned int res = 0;
    for (auto c : str)
    {
        if (c < '0' or c > '9')
            return {};
        res = res * 10 + c - '0';
    }
    return negative ? -res : res;
}

int str_to_int(StringView str)
{
    if (auto val = str_to_int_ifp(str))
        return *val;
    throw runtime_error{str + " is not a number"};
}

template<size_t N>
InplaceString<N> to_string_impl(auto val, auto format)
{
    InplaceString<N> res;
    auto [end, errc] = std::to_chars(res.m_data, res.m_data + N, val, format);
    if (errc != std::errc{})
        throw runtime_error("to_string error");
    res.m_length = end - res.m_data;
    *end = '\0';
    return res;
}

template<size_t N>
InplaceString<N> to_string_impl(auto val)
{
    return to_string_impl<N>(val, 10);
}

InplaceString<15> to_string(int val)
{
    return to_string_impl<15>(val);
}

InplaceString<15> to_string(unsigned val)
{
    return to_string_impl<15>(val);
}

InplaceString<23> to_string(long int val)
{
    return to_string_impl<23>(val);
}

InplaceString<23> to_string(long long int val)
{
    return to_string_impl<23>(val);
}

InplaceString<23> to_string(unsigned long val)
{
    return to_string_impl<23>(val);
}

InplaceString<23> to_string(Hex val)
{
    return to_string_impl<23>(val.val, 16);
}

InplaceString<23> to_string(Grouped val)
{
    auto ungrouped = to_string_impl<23>(val.val);

    InplaceString<23> res;
    for (int pos = 0, len = ungrouped.m_length; pos != len; ++pos)
    {
        if (res.m_length and ((len - pos) % 3) == 0) 
            res.m_data[res.m_length++] = ',';
        res.m_data[res.m_length++] = ungrouped.m_data[pos];
    }
    return res;
}

InplaceString<23> to_string(float val)
{
#if defined(__cpp_lib_to_chars)
    return to_string_impl<23>(val, std::chars_format::general);
#else
    InplaceString<23> res;
    res.m_length = sprintf(res.m_data, "%f", val);
    return res;
#endif
}

InplaceString<7> to_string(Codepoint c)
{
    InplaceString<7> res;
    char* ptr = res.m_data;
    utf8::dump(ptr, c);
    res.m_length = (int)(ptr - res.m_data);
    return res;
}

bool subsequence_match(StringView str, StringView subseq)
{
    auto it = str.begin();
    for (auto& c : subseq)
    {
        if (it == str.end())
            return false;
        while (*it != c)
        {
            if (++it == str.end())
                return false;
        }
        ++it;
    }
    return true;
}

String expand_tabs(StringView line, ColumnCount tabstop, ColumnCount col)
{
    String res;
    res.reserve(line.length());
    for (auto it = line.begin(), end = line.end(); it != end; )
    {
        if (*it == '\t')
        {
            ColumnCount end_col = (col / tabstop + 1) * tabstop;
            res += String{' ', end_col - col};
            col = end_col;
            ++it;
        }
        else
        {
            auto char_beg = it;
            auto cp = utf8::read_codepoint(it, end);
            res += {char_beg, it};
            col += codepoint_width(cp);
        }
    }
    return res;
}

WrapView::Iterator::Iterator(StringView text, ColumnCount max_width)
  : m_remaining{text}, m_max_width{max_width}
{
    if (max_width <= 0)
        throw runtime_error("Invalid max width");
    ++*this;
}

WrapView::Iterator& WrapView::Iterator::operator++()
{
    using Utf8It = utf8::iterator<const char*>;
    Utf8It it{m_remaining.begin(), m_remaining};
    Utf8It last_word_end = it;

    while (it != m_remaining.end())
    {
        const CharCategories cat = categorize(*it, {'_'});
        if (cat == CharCategories::EndOfLine)
        {
            m_current = StringView{m_remaining.begin(), it.base()};
            m_remaining = StringView{(it+1).base(), m_remaining.end()};
            return *this;
        }

        Utf8It word_end = it+1;
        while (word_end != m_remaining.end() and categorize(*word_end, {'_'}) == cat)
            ++word_end;

        if (word_end > m_remaining.begin() and
            utf8::column_distance(m_remaining.begin(), word_end.base()) >= m_max_width)
        {
            auto line_end = last_word_end <= m_remaining.begin() ?
                Utf8It{utf8::advance(m_remaining.begin(), m_remaining.end(), m_max_width), m_remaining}
              : last_word_end;

            m_current = StringView{m_remaining.begin(), line_end.base()};

            while (line_end != m_remaining.end() and is_horizontal_blank(*line_end))
                ++line_end;

            if (line_end != m_remaining.end() and *line_end == '\n')
                ++line_end;

            m_remaining = StringView{line_end.base(), m_remaining.end()};
            return *this;
        }
        if (cat == CharCategories::Word or cat == CharCategories::Punctuation)
            last_word_end = word_end;

        if (word_end > m_remaining.begin())
            it = word_end;
    }
    m_current = m_remaining;
    m_remaining = StringView{};
    return *this;
}

template<typename AppendFunc>
void format_impl(StringView fmt, ArrayView<const StringView> params, AppendFunc append)
{
    int implicitIndex = 0;
    for (auto it = fmt.begin(), end = fmt.end(); it != end;)
    {
        auto opening = std::find(it, end, '{');
        if (opening == end)
        {
            append(StringView{it, opening});
            break;
        }
        else if (opening != it and *(opening-1) == '\\')
        {
            append(StringView{it, opening-1});
            append('{');
            it = opening + 1;
        }
        else
        {
            append(StringView{it, opening});
            auto closing = std::find(opening, end, '}');
            if (closing == end)
                throw runtime_error("format string error, unclosed '{'");

            auto format = std::find(opening+1, closing, ':');
            const int index = opening+1 == format ? implicitIndex : str_to_int({opening+1, format});

            if (index >= params.size())
                throw runtime_error("format string parameter index too big");

            if (format != closing)
            {
                for (ColumnCount width = str_to_int({format+1, closing}), len = params[index].column_length();
                     width > len; --width)
                    append(' ');
            }

            append(params[index]);
            implicitIndex = index+1;
            it = closing+1;
        }
    }
}

StringView format_to(ArrayView<char> buffer, StringView fmt, ArrayView<const StringView> params)
{
    char* ptr = buffer.begin();
    const char* end = buffer.end();
    format_impl(fmt, params, [&](StringView s) mutable {
        for (auto c : s)
        {
            if (ptr == end)
                throw runtime_error("buffer is too small");
            *ptr++ = c;
        }
    });
    if (ptr == end)
        throw runtime_error("buffer is too small");
    *ptr = 0;

    return { buffer.begin(), ptr };
}

void format_with(FunctionRef<void (StringView)> append, StringView fmt, ArrayView<const StringView> params)
{
    format_impl(fmt, params, append);
}

String format(StringView fmt, ArrayView<const StringView> params)
{
    ByteCount size = fmt.length();
    for (auto& s : params) size += s.length();
    String res;
    res.reserve(size);

    format_impl(fmt, params, [&](StringView s) { res += s; });
    return res;
}

String double_up(StringView s, StringView characters)
{
    String res;
    auto pos = s.begin();
    for (auto it = s.begin(), end = s.end(); it != end; ++it)
    {
        if (contains(characters, *it))
        {
            res += StringView{pos, it+1};
            res += *it;
            pos = it+1;
        }
    }
    res += StringView{pos, s.end()};
    return res;
}

UnitTest test_string{[]()
{
    kak_assert(String("youpi ") + "matin" == "youpi matin");

    kak_assert(StringView{"youpi"}.starts_with(""));
    kak_assert(StringView{"youpi"}.starts_with("you"));
    kak_assert(StringView{"youpi"}.starts_with("youpi"));
    kak_assert(not StringView{"youpi"}.starts_with("youpi!"));

    kak_assert(StringView{"youpi"}.ends_with(""));
    kak_assert(StringView{"youpi"}.ends_with("pi"));
    kak_assert(StringView{"youpi"}.ends_with("youpi"));
    kak_assert(not StringView{"youpi"}.ends_with("oup"));

    auto wrapped = "wrap this paragraph\n respecting whitespaces and much_too_long_words" | wrap_at(16) | gather<Vector>();
    kak_assert(wrapped.size() == 6);
    kak_assert(wrapped[0] == "wrap this");
    kak_assert(wrapped[1] == "paragraph");
    kak_assert(wrapped[2] == " respecting");
    kak_assert(wrapped[3] == "whitespaces and");
    kak_assert(wrapped[4] == "much_too_long_wo");
    kak_assert(wrapped[5] == "rds");

    auto wrapped2 = "error: unknown type" | wrap_at(7) | gather<Vector>();
    kak_assert(wrapped2.size() == 3);
    kak_assert(wrapped2[0] == "error:");
    kak_assert(wrapped2[1] == "unknown");
    kak_assert(wrapped2[2] == "type");

    kak_assert(trim_indent(" ") == "");
    kak_assert(trim_indent("no-indent") == "no-indent");
    kak_assert(trim_indent("\nno-indent") == "no-indent");
    kak_assert(trim_indent("\n  indent\n  indent") == "indent\nindent");
    kak_assert(trim_indent("\n  indent\n    indent") == "indent\n  indent");
    kak_assert(trim_indent("\n  indent\n  indent\n   ") == "indent\nindent");

    kak_expect_throw(runtime_error, trim_indent("\n  indent\nno-indent"));

    kak_assert(escape(R"(\youpi:matin:tchou\:)", ":\\", '\\') == R"(\\youpi\:matin\:tchou\\\:)");
    kak_assert(unescape(R"(\\youpi\:matin\:tchou\\\:)", ":\\", '\\') == R"(\youpi:matin:tchou\:)");

    kak_assert(prefix_match("tchou kanaky", "tchou"));
    kak_assert(prefix_match("tchou kanaky", "tchou kanaky"));
    kak_assert(prefix_match("tchou kanaky", "t"));
    kak_assert(not prefix_match("tchou kanaky", "c"));

    kak_assert(subsequence_match("tchou kanaky", "tknky"));
    kak_assert(subsequence_match("tchou kanaky", "knk"));
    kak_assert(subsequence_match("tchou kanaky", "tchou kanaky"));
    kak_assert(not subsequence_match("tchou kanaky", "tchou  kanaky"));

    kak_assert(format("Youhou {1} {} '{0:4}' \\{}", 10, "hehe", 5) == "Youhou hehe 5 '  10' {}");

    char buffer[20];
    kak_assert(format_to(buffer, "Hey {}", 15) == "Hey 15");

    kak_assert(str_to_int("5") == 5);
    kak_assert(str_to_int(to_string(INT_MAX)) == INT_MAX);
    kak_assert(str_to_int(to_string(INT_MIN)) == INT_MIN);
    kak_assert(str_to_int("00") == 0);
    kak_assert(str_to_int("-0") == 0);

    kak_assert(double_up(R"('foo%"bar"')", "'\"%") == R"(''foo%%""bar""'')");

    kak_assert(replace("tchou/tcha/tchi", "/", "!!") == "tchou!!tcha!!tchi");
}};

}
