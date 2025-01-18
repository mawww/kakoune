#include "format.hh"

#include "exception.hh"
#include "string_utils.hh"

#include <algorithm>
#include <charconv>
#include <cstdio>

namespace Kakoune
{


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
    res.m_length = snprintf(res.m_data, 23, "%f", val);
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
                char padding = ' ';
                if (*(++format) == '0')
                {
                    padding = '0';
                    ++format;
                }
                for (ColumnCount width = str_to_int({format, closing}), len = params[index].column_length();
                     width > len; --width)
                    append(padding);
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

}
