#include "string.hh"

#include "exception.hh"
#include "containers.hh"
#include "utf8_iterator.hh"

#include <cstdio>

namespace Kakoune
{

Vector<String> split(StringView str, char separator, char escape)
{
    Vector<String> res;
    auto it = str.begin();
    while (it != str.end())
    {
        res.emplace_back();
        String& element = res.back();
        while (it != str.end())
        {
            auto c = *it;
            if (c == escape and it + 1 != str.end() and *(it+1) == separator)
            {
                element += separator;
                it += 2;
            }
            else if (c == separator)
            {
                ++it;
                break;
            }
            else
            {
                element += c;
                ++it;
            }
        }
    }
    return res;
}

Vector<StringView> split(StringView str, char separator)
{
    Vector<StringView> res;
    auto beg = str.begin();
    for (auto it = beg; it != str.end(); ++it)
    {
        if (*it == separator)
        {
            res.emplace_back(beg, it);
            beg = it + 1;
        }
    }
    res.emplace_back(beg, str.end());
    return res;
}

String escape(StringView str, StringView characters, char escape)
{
    String res;
    res.reserve(str.length());
    for (auto& c : str)
    {
        if (contains(characters, c))
            res += escape;
        res += c;
    }
    return res;
}

String unescape(StringView str, StringView characters, char escape)
{
    String res;
    res.reserve(str.length());
    for (auto& c : str)
    {
        if (contains(characters, c) and not res.empty() and res.back() == escape)
            res.back() = c;
        else
            res += c;
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

int str_to_int(StringView str)
{
    unsigned int res = 0;
    bool negative = false;
    for (auto it = str.begin(), end = str.end(); it != end; ++it)
    {
        const char c = *it;
        if (it == str.begin() and c == '-')
            negative = true;
        else if (c >= '0' and c <= '9')
            res = res * 10 + c - '0';
        else
            throw runtime_error(str + "is not a number");
    }
    return negative ? -(int)res : (int)res;
}

InplaceString<16> to_string(int val)
{
    InplaceString<16> res;
    res.m_length = sprintf(res.m_data, "%i", val);
    return res;
}

InplaceString<24> to_string(size_t val)
{
    InplaceString<24> res;
    res.m_length = sprintf(res.m_data, "%zu", val);
    return res;
}

InplaceString<24> to_string(float val)
{
    InplaceString<24> res;
    res.m_length = sprintf(res.m_data, "%f", val);
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

String expand_tabs(StringView line, CharCount tabstop, CharCount col)
{
    String res;
    res.reserve(line.length());
    for (auto it = line.begin(), end = line.end(); it != end; )
    {
        if (*it == '\t')
        {
            CharCount end_col = (col / tabstop + 1) * tabstop;
            res += String{' ', end_col - col};
            col = end_col;
            ++it;
        }
        else
        {
            auto char_end = utf8::next(it, end);
            res += {it, char_end};
            ++col;
            it = char_end;
        }
    }
    return res;
}

Vector<StringView> wrap_lines(StringView text, CharCount max_width)
{
    using Utf8It = utf8::iterator<const char*>;
    Utf8It word_begin{text.begin()};
    Utf8It word_end{word_begin};
    Utf8It end{text.end()};
    CharCount col = 0;
    Vector<StringView> lines;
    Utf8It line_begin = text.begin();
    Utf8It line_end = line_begin;
    while (word_begin != end)
    {
        const CharCategories cat = categorize(*word_begin);
        do
        {
            ++word_end;
        } while (word_end != end and categorize(*word_end) == cat);

        col += word_end - word_begin;
        if ((word_begin != line_begin and col > max_width) or
            cat == CharCategories::EndOfLine)
        {
            lines.emplace_back(line_begin.base(), line_end.base());
            line_begin = (cat == CharCategories::EndOfLine or
                          cat == CharCategories::Blank) ? word_end : word_begin;
            col = word_end - line_begin;
        }
        if (cat == CharCategories::Word or cat == CharCategories::Punctuation)
            line_end = word_end;

        word_begin = word_end;
    }
    if (line_begin != word_begin)
        lines.emplace_back(line_begin.base(), word_begin.base());
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
            int index;
            if (closing == opening + 1)
                index = implicitIndex;
            else
                index = str_to_int({opening+1, closing});

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

}
