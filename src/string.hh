#ifndef string_hh_INCLUDED
#define string_hh_INCLUDED

#include "array_view.hh"
#include "hash.hh"
#include "optional.hh"
#include "units.hh"
#include "utf8.hh"
#include "vector.hh"

#include <string>
#include <climits>

namespace Kakoune
{

class StringView;

template<typename Type, typename CharType>
class StringOps
{
public:
    using value_type = CharType;

    friend inline size_t hash_value(const Type& str)
    {
        return hash_data(str.data(), (int)str.length());
    }

    using iterator = CharType*;
    using const_iterator = const CharType*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    [[gnu::always_inline]]
    iterator begin() { return type().data(); }

    [[gnu::always_inline]]
    const_iterator begin() const { return type().data(); }

    [[gnu::always_inline]]
    iterator end() { return type().data() + (int)type().length(); }

    [[gnu::always_inline]]
    const_iterator end() const { return type().data() + (int)type().length(); }

    reverse_iterator rbegin() { return reverse_iterator{end()}; }
    const_reverse_iterator rbegin() const { return const_reverse_iterator{end()}; }

    reverse_iterator rend() { return reverse_iterator{begin()}; }
    const_reverse_iterator rend() const { return const_reverse_iterator{begin()}; }

    CharType& front() { return *type().data(); }
    const CharType& front() const { return *type().data(); }
    CharType& back() { return type().data()[(int)type().length() - 1]; }
    const CharType& back() const { return type().data()[(int)type().length() - 1]; }

    [[gnu::always_inline]]
    CharType& operator[](ByteCount pos) { return type().data()[(int)pos]; }

    [[gnu::always_inline]]
    const CharType& operator[](ByteCount pos) const { return type().data()[(int)pos]; }

    Codepoint operator[](CharCount pos) const
    { return utf8::codepoint(utf8::advance(begin(), end(), pos), end()); }

    CharCount char_length() const { return utf8::distance(begin(), end()); }

    [[gnu::always_inline]]
    bool empty() const { return type().length() == 0_byte; }

    ByteCount byte_count_to(CharCount count) const
    { return utf8::advance(begin(), end(), (int)count) - begin(); }

    CharCount char_count_to(ByteCount count) const
    { return utf8::distance(begin(), begin() + (int)count); }

    StringView substr(ByteCount from, ByteCount length = INT_MAX) const;
    StringView substr(CharCount from, CharCount length = INT_MAX) const;

private:
    [[gnu::always_inline]]
    Type& type() { return *static_cast<Type*>(this); }
    [[gnu::always_inline]]
    const Type& type() const { return *static_cast<const Type*>(this); }
};

class String : public StringOps<String, char>
{
public:
    using Content = std::basic_string<char, std::char_traits<char>,
                                      Allocator<char, MemoryDomain::String>>;

    String() {}
    String(const char* content) : m_data(content) {}
    String(const char* content, ByteCount len) : m_data(content, (size_t)(int)len) {}
    explicit String(char content, CharCount count = 1) : m_data((size_t)(int)count, content) {}
    explicit String(Codepoint cp, CharCount count = 1)
    {
        while (count-- > 0)
            utf8::dump(std::back_inserter(*this), cp);
    }
    template<typename Iterator>
    String(Iterator begin, Iterator end) : m_data(begin, end) {}

    [[gnu::always_inline]]
    char* data() { return &m_data[0]; }

    [[gnu::always_inline]]
    const char* data() const { return m_data.data(); }

    [[gnu::always_inline]]
    ByteCount length() const { return m_data.length(); }

    [[gnu::always_inline]]
    const char* c_str() const { return m_data.c_str(); }

    [[gnu::always_inline]]
    void append(const char* data, ByteCount count) { m_data.append(data, (size_t)(int)count); }

    void clear() { m_data.clear(); }

    void push_back(char c) { m_data.push_back(c); }
    void resize(ByteCount size) { m_data.resize((size_t)(int)size); }
    void reserve(ByteCount size) { m_data.reserve((size_t)(int)size); }

    static const String ms_empty;

private:
    Content m_data;
};

class StringView : public StringOps<StringView, const char>
{
public:
    constexpr StringView() = default;
    constexpr StringView(const char* data, ByteCount length)
        : m_data{data}, m_length{length} {}
    constexpr StringView(const char* data) : m_data{data}, m_length{data ? strlen(data) : 0} {}
    constexpr StringView(const char* begin, const char* end) : m_data{begin}, m_length{(int)(end - begin)} {}
    StringView(const String& str) : m_data{str.data()}, m_length{(int)str.length()} {}
    StringView(const char& c) : m_data(&c), m_length(1) {}
    StringView(int c) = delete;

    [[gnu::always_inline]]
    constexpr const char* data() const { return m_data; }

    [[gnu::always_inline]]
    constexpr ByteCount length() const { return m_length; }

    String str() const { return {begin(), end()}; }

    struct ZeroTerminatedString
    {
        ZeroTerminatedString(const char* begin, const char* end)
        {
            if (*end == '\0')
                unowned = begin;
            else
                owned = String::Content(begin, end);
        }
        operator const char*() const { return unowned ? unowned : owned.c_str(); }

    private:
        String::Content owned;
        const char* unowned = nullptr;

    };
    ZeroTerminatedString zstr() const { return {begin(), end()}; }

    [[gnu::optimize(3)]] // this is recursive for constexpr reason
    static constexpr ByteCount strlen(const char* s)
    {
        return *s == 0 ? 0 : strlen(s+1) + 1;
    }

private:
    const char* m_data = nullptr;
    ByteCount m_length = 0;
};

template<typename Type, typename CharType>
inline StringView StringOps<Type, CharType>::substr(ByteCount from, ByteCount length) const
{
    if (length < 0)
        length = INT_MAX;
    return StringView{ type().data() + (int)from, std::min(type().length() - from, length) };
}

template<typename Type, typename CharType>
inline StringView StringOps<Type, CharType>::substr(CharCount from, CharCount length) const
{
    if (length < 0)
        length = INT_MAX;
    auto beg = utf8::advance(begin(), end(), (int)from);
    return StringView{ beg, utf8::advance(beg, end(), length) };
}

inline String& operator+=(String& lhs, StringView rhs)
{
    lhs.append(rhs.data(), rhs.length());
    return lhs;
}

inline String operator+(StringView lhs, StringView rhs)
{
    String res;
    res.reserve((int)(lhs.length() + rhs.length()));
    res.append(lhs.data(), lhs.length());
    res.append(rhs.data(), rhs.length());
    return res;
}

[[gnu::always_inline]]
inline bool operator==(const StringView& lhs, const StringView& rhs)
{
    return lhs.length() == rhs.length() and
       std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

[[gnu::always_inline]]
inline bool operator!=(const StringView& lhs, const StringView& rhs)
{ return not (lhs == rhs); }

inline bool operator<(const StringView& lhs, const StringView& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(),
                                        rhs.begin(), rhs.end());
}

Vector<String> split(StringView str, char separator, char escape);
Vector<StringView> split(StringView str, char separator);

String escape(StringView str, StringView characters, char escape);
String unescape(StringView str, StringView characters, char escape);

String indent(StringView str, StringView indent = "    ");

template<typename Container>
String join(const Container& container, char joiner, bool esc_joiner = true)
{
    String res;
    for (const auto& str : container)
    {
        if (not res.empty())
            res += joiner;
        res += esc_joiner ? escape(str, joiner, '\\') : str;
    }
    return res;
}

inline String operator"" _str(const char* str, size_t)
{
    return String(str);
}

inline String codepoint_to_str(Codepoint cp)
{
    String str;
    utf8::dump(std::back_inserter(str), cp);
    return str;
}

int str_to_int(StringView str); // throws on error
Optional<int> str_to_int_ifp(StringView str);

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
inline Hex hex(size_t val) { return {val}; }

InplaceString<15> to_string(int val);
InplaceString<23> to_string(long int val);
InplaceString<23> to_string(size_t val);
InplaceString<23> to_string(Hex val);
InplaceString<23> to_string(float val);
InplaceString<7>  to_string(Codepoint c);

template<typename RealType, typename ValueType>
decltype(to_string(std::declval<ValueType>()))
to_string(const StronglyTypedNumber<RealType, ValueType>& val)
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

namespace detail
{

template<typename T> using IsString = std::is_convertible<T, StringView>;

template<typename T, class = typename std::enable_if<!IsString<T>::value>::type>
auto format_param(const T& val) -> decltype(to_string(val)) { return to_string(val); }

template<typename T, class = typename std::enable_if<IsString<T>::value>::type>
StringView format_param(const T& val) { return val; }

}

String format(StringView fmt, ArrayView<const StringView> params);

template<typename... Types>
String format(StringView fmt, Types... params)
{
    return format(fmt, ArrayView<const StringView>{detail::format_param(params)...});
}

StringView format_to(ArrayView<char> buffer, StringView fmt, ArrayView<const StringView> params);

template<typename... Types>
StringView format_to(ArrayView<char> buffer, StringView fmt, Types... params)
{
    return format_to(buffer, fmt, ArrayView<const StringView>{detail::format_param(params)...});
}

}

#endif // string_hh_INCLUDED
