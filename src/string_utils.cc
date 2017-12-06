#include "string_utils.hh"

#include "exception.hh"
#include "utf8_iterator.hh"
#include "unit_tests.hh"

namespace Kakoune
{

StringView trim_whitespaces(StringView str)
{
    auto beg = str.begin(), end = str.end();
    while (beg != end and is_blank(*beg))
        ++beg;
    while (beg != end and is_blank(*(end-1)))
        --end;
    return {beg, end};
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

Optional<int> str_to_int_ifp(StringView str)
{
    bool negative = not str.empty() and str[0] == '-';
    if (negative)
        str = str.substr(1_byte);

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

InplaceString<15> to_string(int val)
{
    InplaceString<15> res;
    res.m_length = sprintf(res.m_data, "%i", val);
    return res;
}

InplaceString<23> to_string(long int val)
{
    InplaceString<23> res;
    res.m_length = sprintf(res.m_data, "%li", val);
    return res;
}

InplaceString<23> to_string(long long int val)
{
    InplaceString<23> res;
    res.m_length = sprintf(res.m_data, "%lli", val);
    return res;
}

InplaceString<23> to_string(size_t val)
{
    InplaceString<23> res;
    res.m_length = sprintf(res.m_data, "%zu", val);
    return res;
}

InplaceString<23> to_string(Hex val)
{
    InplaceString<23> res;
    res.m_length = sprintf(res.m_data, "%zx", val.val);
    return res;
}

InplaceString<23> to_string(float val)
{
    InplaceString<23> res;
    res.m_length = sprintf(res.m_data, "%f", val);
    return res;
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

Vector<StringView> wrap_lines(StringView text, ColumnCount max_width)
{
    if (max_width <= 0)
        throw runtime_error("Invalid max width");

    using Utf8It = utf8::iterator<const char*>;
    Utf8It it{text.begin(), text};
    Utf8It end{text.end(), text};
    Utf8It line_begin = it;
    Utf8It last_word_end = it;

    Vector<StringView> lines;
    while (it != end)
    {
        const CharCategories cat = categorize(*it, {});
        if (cat == CharCategories::EndOfLine)
        {
            lines.emplace_back(line_begin.base(), it.base());
            line_begin = it = it+1;
            continue;
        }

        Utf8It word_end = it+1;
        while (word_end != end and categorize(*word_end, {}) == cat)
            ++word_end;

        while (word_end > line_begin and
               utf8::column_distance(line_begin.base(), word_end.base()) >= max_width)
        {
            auto line_end = last_word_end <= line_begin ?
                Utf8It{utf8::advance(line_begin.base(), text.end(), max_width), text}
              : last_word_end;

            lines.emplace_back(line_begin.base(), line_end.base());

            while (line_end != end and is_horizontal_blank(*line_end))
                ++line_end;

            if (line_end != end and *line_end == '\n')
                ++line_end;

            it = line_begin = line_end;
        }
        if (cat == CharCategories::Word or cat == CharCategories::Punctuation)
            last_word_end = word_end;

        if (word_end > line_begin)
            it = word_end;
    }
    if (line_begin != end)
        lines.emplace_back(line_begin.base(), text.end());
    return lines;
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
                throw runtime_error("Format string error, unclosed '{'");

            const int index = (closing == opening + 1) ?
                implicitIndex : str_to_int({opening+1, closing});

            if (index >= params.size())
                throw runtime_error("Format string parameter index too big");

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

String format(StringView fmt, ArrayView<const StringView> params)
{
    ByteCount size = fmt.length();
    for (auto& s : params) size += s.length();
    String res;
    res.reserve(size);

    format_impl(fmt, params, [&](StringView s) { res += s; });
    return res;
}

UnitTest test_string{[]()
{
    kak_assert(String("youpi ") + "matin" == "youpi matin");

    Vector<StringView> wrapped = wrap_lines("wrap this paragraph\n respecting whitespaces and much_too_long_words", 16);
    kak_assert(wrapped.size() == 6);
    kak_assert(wrapped[0] == "wrap this");
    kak_assert(wrapped[1] == "paragraph");
    kak_assert(wrapped[2] == " respecting");
    kak_assert(wrapped[3] == "whitespaces and");
    kak_assert(wrapped[4] == "much_too_long_wo");
    kak_assert(wrapped[5] == "rds");

    Vector<StringView> wrapped2 = wrap_lines("error: unknown type", 7);
    kak_assert(wrapped2.size() == 3);
    kak_assert(wrapped2[0] == "error:");
    kak_assert(wrapped2[1] == "unknown");
    kak_assert(wrapped2[2] == "type");

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

    kak_assert(format("Youhou {1} {} {0} \\{}", 10, "hehe", 5) == "Youhou hehe 5 10 {}");

    char buffer[20];
    kak_assert(format_to(buffer, "Hey {}", 15) == "Hey 15");

    kak_assert(str_to_int("5") == 5);
    kak_assert(str_to_int(to_string(INT_MAX)) == INT_MAX);
    kak_assert(str_to_int(to_string(INT_MIN)) == INT_MIN);
    kak_assert(str_to_int("00") == 0);
    kak_assert(str_to_int("-0") == 0);

    kak_assert(replace("tchou/tcha/tchi", "/", "!!") == "tchou!!tcha!!tchi");
}};

}
