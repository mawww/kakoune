#ifndef string_hh_INCLUDED
#define string_hh_INCLUDED

#include "units.hh"
#include "utf8.hh"
#include "hash.hh"
#include "vector.hh"

#include <string>
#include <climits>
#include <cstring>

namespace Kakoune
{

class StringView;

using StringBase = std::basic_string<char, std::char_traits<char>,
                                     Allocator<char, MemoryDomain::String>>;

class String : public  StringBase
{
public:
    String() {}
    String(const char* content) : StringBase(content) {}
    String(StringBase content) : StringBase(std::move(content)) {}
    template<typename Char, typename Traits, typename Alloc>
    String(const std::basic_string<Char, Traits, Alloc>& content) : StringBase(content.begin(), content.end()) {}
    explicit String(char content, CharCount count = 1) : StringBase((size_t)(int)count, content) {}
    explicit String(Codepoint cp, CharCount count = 1)
    {
        while (count-- > 0)
            utf8::dump(back_inserter(*this), cp);
    }
    template<typename Iterator>
    String(Iterator begin, Iterator end) : StringBase(begin, end) {}

    StringBase& stdstr() { return *this; }
    const StringBase& stdstr() const { return *this; }

    [[gnu::always_inline]]
    char      operator[](ByteCount pos) const { return StringBase::operator[]((int)pos); }
    [[gnu::always_inline]]
    char&     operator[](ByteCount pos) { return StringBase::operator[]((int)pos); }
    Codepoint operator[](CharCount pos) { return utf8::codepoint(utf8::advance(begin(), end(), pos), end()); }

    [[gnu::always_inline]]
    ByteCount length() const { return ByteCount{(int)StringBase::length()}; }
    CharCount char_length() const { return utf8::distance(begin(), end()); }
    ByteCount byte_count_to(CharCount count) const { return utf8::advance(begin(), end(), (int)count) - begin(); }
    CharCount char_count_to(ByteCount count) const { return utf8::distance(begin(), begin() + (int)count); }

    String& operator+=(const String& other) { StringBase::operator+=(other); return *this; }
    String& operator+=(const char* other) { StringBase::operator+=(other); return *this; }
    String& operator+=(char other) { StringBase::operator+=(other); return *this; }
    String& operator+=(Codepoint cp) { utf8::dump(back_inserter(*this), cp); return *this; }

    StringView substr(ByteCount pos, ByteCount length = INT_MAX) const;
    StringView substr(CharCount pos, CharCount length = INT_MAX) const;
};

class StringView
{
public:
    constexpr StringView() : m_data{nullptr}, m_length{0} {}
    constexpr StringView(const char* data, ByteCount length)
        : m_data{data}, m_length{length} {}
    StringView(const char* data) : m_data{data}, m_length{(int)strlen(data)} {}
    constexpr StringView(const char* begin, const char* end) : m_data{begin}, m_length{(int)(end - begin)} {}
    template<typename Char, typename Traits, typename Alloc>
    StringView(const std::basic_string<Char, Traits, Alloc>& str) : m_data{str.data()}, m_length{(int)str.length()} {}
    StringView(const char& c) : m_data(&c), m_length(1) {}

    friend bool operator==(StringView lhs, StringView rhs);

    [[gnu::always_inline]]
    const char* data() const { return m_data; }

    using iterator = const char*;
    using reverse_iterator = std::reverse_iterator<const char*>;

    [[gnu::always_inline]]
    iterator begin() const { return m_data; }
    [[gnu::always_inline]]
    iterator end() const { return m_data + (int)m_length; }

    reverse_iterator rbegin() const { return reverse_iterator{m_data + (int)m_length}; }
    reverse_iterator rend() const { return reverse_iterator{m_data}; }

    char front() const { return *m_data; }
    char back() const { return m_data[(int)m_length - 1]; }

    [[gnu::always_inline]]
    char operator[](ByteCount pos) const { return m_data[(int)pos]; }
    Codepoint operator[](CharCount pos) { return utf8::codepoint(utf8::advance(begin(), end(), pos), end()); }

    [[gnu::always_inline]]
    ByteCount length() const { return m_length; }
    CharCount char_length() const { return utf8::distance(begin(), end()); }

    [[gnu::always_inline]]
    bool empty() { return m_length == 0_byte; }

    ByteCount byte_count_to(CharCount count) const;
    CharCount char_count_to(ByteCount count) const;

    StringView substr(ByteCount from, ByteCount length = INT_MAX) const;
    StringView substr(CharCount from, CharCount length = INT_MAX) const;

    String str() const { return String{begin(), end()}; }

    operator String() const { return str(); } // to remove

    struct ZeroTerminatedString
    {
        ZeroTerminatedString(const char* begin, const char* end)
        {
            if (*end == '\0')
                unowned = begin;
            else
                owned = StringBase(begin, end);
        }
        operator const char*() const { return unowned ? unowned : owned.c_str(); }

    private:
        StringBase owned;
        const char* unowned = nullptr;

    };
    ZeroTerminatedString zstr() const { return ZeroTerminatedString{begin(), end()}; }

private:
    const char* m_data;
    ByteCount m_length;
};

inline bool operator==(StringView lhs, StringView rhs)
{
    return lhs.m_length == rhs.m_length and memcmp(lhs.m_data, rhs.m_data, (int)lhs.m_length) == 0;
}

inline bool operator!=(StringView lhs, StringView rhs)
{
    return not (lhs == rhs);
}

bool operator<(StringView lhs, StringView rhs);

inline ByteCount StringView::byte_count_to(CharCount count) const
{
    return utf8::advance(begin(), end(), (int)count) - begin();
}

inline CharCount StringView::char_count_to(ByteCount count) const
{
    return utf8::distance(begin(), begin() + (int)count);
}

inline StringView StringView::substr(ByteCount from, ByteCount length) const
{
    if (length < 0)
        length = INT_MAX;
    return StringView{ m_data + (int)from, std::min(m_length - from, length) };
}

inline StringView StringView::substr(CharCount from, CharCount length) const
{
    if (length < 0)
        length = INT_MAX;
    auto beg = utf8::advance(begin(), end(), (int)from);
    return StringView{ beg, utf8::advance(beg, end(), length) };
}

inline StringView String::substr(ByteCount pos, ByteCount length) const
{
    return StringView{*this}.substr(pos, length);
}

inline StringView String::substr(CharCount pos, CharCount length) const
{
    return StringView{*this}.substr(pos, length);
}

inline String& operator+=(String& lhs, StringView rhs)
{
    lhs.append(rhs.data(), (size_t)(int)rhs.length());
    return lhs;
}

inline String operator+(const String& lhs, const String& rhs)
{
    String res = lhs;
    res += rhs;
    return res;
}

inline String operator+(StringView lhs, StringView rhs)
{
    String res{lhs.begin(), lhs.end()};
    res += rhs;
    return res;
}

inline String operator+(const String& lhs, StringView rhs)
{
    String res = lhs;
    res += rhs;
    return res;
}

inline String operator+(StringView lhs, const String& rhs)
{
    String res{lhs.begin(), lhs.end()};
    res.append(rhs);
    return res;
}

inline String operator+(const char* lhs, StringView rhs)
{
    return StringView{lhs} + rhs;
}

inline String operator+(StringView lhs, const char* rhs)
{
    return lhs + StringView{rhs};
}

inline String operator+(const char* lhs, const String& rhs)
{
    return StringView{lhs} + rhs;
}

inline String operator+(const String& lhs, const char* rhs)
{
    return lhs + StringView{rhs};
}

Vector<String> split(StringView str, char separator, char escape);
Vector<StringView> split(StringView str, char separator);

String escape(StringView str, StringView characters, char escape);
String unescape(StringView str, StringView characters, char escape);

template<typename Container>
String join(const Container& container, char joiner)
{
    String res;
    for (const auto& str : container)
    {
        if (not res.empty())
            res += joiner;
        res += escape(str, joiner, '\\');
    }
    return res;
}

inline String operator"" _str(const char* str, size_t)
{
    return String(str);
}

inline StringView operator"" _sv(const char* str, size_t len)
{
    return StringView(str, (int)len);
}

inline String codepoint_to_str(Codepoint cp)
{
    StringBase str;
    utf8::dump(back_inserter(str), cp);
    return String(str);
}

int str_to_int(StringView str);

String to_string(int val);
String to_string(size_t val);
String to_string(float val);

template<typename RealType, typename ValueType>
String to_string(const StronglyTypedNumber<RealType, ValueType>& val)
{
    return to_string((ValueType)val);
}

inline bool prefix_match(StringView str, StringView prefix)
{
    return str.substr(0_byte, prefix.length()) == prefix;
}

bool subsequence_match(StringView str, StringView subseq);

String expand_tabs(StringView line, CharCount tabstop, CharCount col = 0);

Vector<StringView> wrap_lines(StringView text, CharCount max_width);

inline size_t hash_value(const Kakoune::String& str)
{
    return hash_data(str.data(), (int)str.length());
}

inline size_t hash_value(const Kakoune::StringView& str)
{
    return hash_data(str.data(), (int)str.length());
}

}

#endif // string_hh_INCLUDED
