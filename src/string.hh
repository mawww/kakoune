#ifndef string_hh_INCLUDED
#define string_hh_INCLUDED

#include "memory.hh"
#include "hash.hh"
#include "units.hh"
#include "utf8.hh"

#include <climits>

namespace Kakoune
{

class StringView;

template<typename Type, typename CharType>
class StringOps
{
public:
    using value_type = CharType;

    friend constexpr size_t hash_value(const Type& str)
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
    ColumnCount column_length() const { return utf8::column_distance(begin(), end()); }

    [[gnu::always_inline]]
    bool empty() const { return type().length() == 0_byte; }

    bool starts_with(StringView str) const;
    bool ends_with(StringView str) const;

    ByteCount byte_count_to(CharCount count) const
    { return utf8::advance(begin(), end(), count) - begin(); }

    ByteCount byte_count_to(ColumnCount count) const
    { return utf8::advance(begin(), end(), count) - begin(); }

    CharCount char_count_to(ByteCount count) const
    { return utf8::distance(begin(), begin() + (int)count); }

    ColumnCount column_count_to(ByteCount count) const
    { return utf8::column_distance(begin(), begin() + (int)count); }

    StringView substr(ByteCount from, ByteCount length = INT_MAX) const;
    StringView substr(CharCount from, CharCount length = INT_MAX) const;
    StringView substr(ColumnCount from, ColumnCount length = INT_MAX) const;

private:
    [[gnu::always_inline]]
    Type& type() { return *static_cast<Type*>(this); }
    [[gnu::always_inline]]
    const Type& type() const { return *static_cast<const Type*>(this); }
};

constexpr ByteCount strlen(const char* s)
{
    int i = 0;
    while (*s++ != 0)
        ++i;
    return {i};
}

class String : public StringOps<String, char>
{
public:
    String() {}
    String(const char* content) : m_data(content, (size_t)strlen(content)) {}
    String(const char* content, ByteCount len) : m_data(content, (size_t)len) {}
    explicit String(Codepoint cp, CharCount count = 1)
    {
        reserve(utf8::codepoint_size(cp) * (int)count);
        while (count-- > 0)
            utf8::dump(std::back_inserter(*this), cp);
    }
    explicit String(Codepoint cp, ColumnCount count)
    {
        int cp_count = (int)(count / std::max(codepoint_width(cp), 1_col));
        reserve(utf8::codepoint_size(cp) * cp_count);
        while (cp_count-- > 0)
            utf8::dump(std::back_inserter(*this), cp);
    }
    String(const char* begin, const char* end) : m_data(begin, end-begin) {}

    explicit String(StringView str);

    struct NoCopy{};
    String(NoCopy, StringView str);

    static String no_copy(StringView str);

    [[gnu::always_inline]]
    char* data() { return m_data.data(); }

    [[gnu::always_inline]]
    const char* data() const { return m_data.data(); }

    [[gnu::always_inline]]
    ByteCount length() const { return m_data.size(); }

    [[gnu::always_inline]]
    const char* c_str() const { return m_data.data(); }

    [[gnu::always_inline]]
    void append(const char* data, ByteCount count) { m_data.append(data, (size_t)count); }

    void clear() { m_data.clear(); }

    void push_back(char c) { m_data.append(&c, 1); }
    void force_size(ByteCount size) { m_data.force_size((size_t)size); }
    void reserve(ByteCount size) { m_data.reserve((size_t)size); }
    void resize(ByteCount size, char c);

    static const String ms_empty;
    static constexpr const char* option_type_name = "str";

    // String data storage using small string optimization.
    //
    // the LSB of the last byte is used to flag if we are using the small buffer
    // or an allocated one. On big endian systems that means the allocated
    // capacity must be pair, on little endian systems that means the allocated
    // capacity cannot use its most significant byte, so we effectively limit
    // capacity to 2^24 on 32bit arch, and 2^60 on 64.
    struct Data
    {
        using Alloc = Allocator<char, MemoryDomain::String>;

        Data() { set_empty(); }
        Data(NoCopy, const char* data, size_t size) : u{Long{const_cast<char*>(data), size, 0}} {}

        Data(const char* data, size_t size, size_t capacity);
        Data(const char* data, size_t size) : Data(data, size, size) {}
        Data(const Data& other) : Data{other.data(), other.size()} {}

        ~Data() { release(); }
        Data(Data&& other) noexcept : u{other.u} { other.set_empty(); }
        Data& operator=(const Data& other);
        Data& operator=(Data&& other) noexcept;

        bool is_long() const { return (u.s.size & 1) == 0; }
        size_t size() const { return is_long() ? u.l.size : (u.s.size >> 1); }
        size_t capacity() const { return is_long() ? u.l.capacity : Short::capacity; }

        const char* data() const { return is_long() ? u.l.ptr : u.s.string; }
        char* data() { return is_long() ? u.l.ptr : u.s.string; }

        template<bool copy = true>
        void reserve(size_t new_capacity);
        void set_size(size_t size);
        void force_size(size_t new_size);
        void append(const char* str, size_t len);
        void clear();

    private:
        struct Long
        {
            static constexpr size_t max_capacity =
                (size_t)1 << 8 * (sizeof(size_t) - 1);

            char* ptr;
            size_t size;
            size_t capacity;
        };

        struct Short
        {
            static constexpr size_t capacity = sizeof(Long) - 2;
            char string[capacity+1];
            unsigned char size;
        };

        union
        {
            Long l;
            Short s;
        } u;

        void release()
        {
            if (is_long() and u.l.capacity != 0)
                Alloc{}.deallocate(u.l.ptr, u.l.capacity+1);
        }

        void set_empty() { u.s.size = 1; u.s.string[0] = 0; }
        void set_short(const char* data, size_t size);
    };

private:
    Data m_data;
};

class StringView : public StringOps<StringView, const char>
{
public:
    StringView() = default;
    constexpr StringView(const char* data, ByteCount length)
        : m_data{data}, m_length{length} {}
    constexpr StringView(const char* data) : m_data{data}, m_length{data ? strlen(data) : 0} {}
    constexpr StringView(const char* begin, const char* end) : m_data{begin}, m_length{(int)(end - begin)} {}
    StringView(const String& str) : m_data{str.data()}, m_length{(int)str.length()} {}
    StringView(const char& c) : m_data(&c), m_length(1) {}
    StringView(int c) = delete;
    StringView(Codepoint c) = delete;

    [[gnu::always_inline]]
    constexpr const char* data() const { return m_data; }

    [[gnu::always_inline]]
    constexpr ByteCount length() const { return m_length; }

    String str() const { return {m_data, m_length}; }

    struct ZeroTerminatedString
    {
        ZeroTerminatedString(const char* begin, const char* end)
        {
            if (*end == '\0')
                unowned = begin;
            else
                owned = String::Data(begin, end - begin);
        }
        operator const char*() const { return unowned ? unowned : owned.data(); }

    private:
        String::Data owned;
        const char* unowned = nullptr;
    };
    ZeroTerminatedString zstr() const { return {begin(), end()}; }

private:
    const char* m_data;
    ByteCount m_length;
};

static_assert(std::is_trivial<StringView>::value, "");

template<> struct HashCompatible<String, StringView> : std::true_type {};
template<> struct HashCompatible<StringView, String> : std::true_type {};

inline String::String(StringView str) : String{str.begin(), str.length()} {}
inline String::String(NoCopy, StringView str) : m_data{NoCopy{}, str.begin(), (size_t)str.length()} {}
inline String String::no_copy(StringView str) { return {NoCopy{}, str}; }

template<typename Type, typename CharType>
inline StringView StringOps<Type, CharType>::substr(ByteCount from, ByteCount length) const
{
    if (length < 0)
        length = INT_MAX;
    const auto str_len = type().length();
    kak_assert(from >= 0 and from <= str_len);
    return StringView{ type().data() + (int)from, std::min(str_len - from, length) };
}

template<typename Type, typename CharType>
inline StringView StringOps<Type, CharType>::substr(CharCount from, CharCount length) const
{
    if (length < 0)
        length = INT_MAX;
    auto beg = utf8::advance(begin(), end(), from);
    return StringView{ beg, utf8::advance(beg, end(), length) };
}

template<typename Type, typename CharType>
inline StringView StringOps<Type, CharType>::substr(ColumnCount from, ColumnCount length) const
{
    if (length < 0)
        length = INT_MAX;
    auto beg = utf8::advance(begin(), end(), from);
    return StringView{ beg, utf8::advance(beg, end(), length) };
}

template<typename Type, typename CharType>
inline bool StringOps<Type, CharType>::starts_with(StringView str) const
{
    if (type().length() < str.length())
        return false;
    return substr(0, str.length()) == str;
}

template<typename Type, typename CharType>
inline bool StringOps<Type, CharType>::ends_with(StringView str) const
{
    if (type().length() < str.length())
        return false;
    return substr(type().length() - str.length()) == str;
}

inline String& operator+=(String& lhs, StringView rhs)
{
    lhs.append(rhs.data(), rhs.length());
    return lhs;
}

inline String operator+(StringView lhs, StringView rhs)
{
    String res;
    res.reserve(lhs.length() + rhs.length());
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

inline String operator"" _str(const char* str, size_t)
{
    return String(str);
}

inline StringView operator"" _sv(const char* str, size_t)
{
    return StringView{str};
}

}

#endif // string_hh_INCLUDED
