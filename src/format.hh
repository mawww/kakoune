#ifndef format_hh_INCLUDED
#define format_hh_INCLUDED

#include "string.hh"
#include "utils.hh"

namespace Kakoune
{

template<size_t N>
struct InplaceString
{
    static_assert(N < 256, "InplaceString cannot handle sizes >= 256");

    constexpr operator StringView() const { return {m_data, ByteCount{m_length}}; }
    operator String() const { return {m_data, ByteCount{m_length}}; }

    unsigned char m_length{};
    char m_data[N];
};

struct Hex { size_t val; };
constexpr Hex hex(size_t val) { return {val}; }

struct Grouped { size_t val; };
constexpr Grouped grouped(size_t val) { return {val}; }

InplaceString<15> to_string(int val);
InplaceString<15> to_string(unsigned val);
InplaceString<23> to_string(long int val);
InplaceString<23> to_string(unsigned long val);
InplaceString<23> to_string(long long int val);
InplaceString<23> to_string(Hex val);
InplaceString<23> to_string(Grouped val);
InplaceString<23> to_string(float val);
InplaceString<7>  to_string(Codepoint c);

template<typename RealType, typename ValueType>
decltype(auto) to_string(const StronglyTypedNumber<RealType, ValueType>& val)
{
    return to_string((ValueType)val);
}

namespace detail
{

template<typename T> requires std::is_convertible_v<T, StringView>
StringView format_param(const T& val) { return val; }

template<typename T> requires (not std::is_convertible_v<T, StringView>)
decltype(auto) format_param(const T& val) { return to_string(val); }

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

void format_with(FunctionRef<void (StringView)> append, StringView fmt, ArrayView<const StringView> params);

template<typename... Types>
void format_with(FunctionRef<void (StringView)> append, StringView fmt, Types&&... params)
{
    return format_with(append, fmt, ArrayView<const StringView>{detail::format_param(std::forward<Types>(params))...});
}

}

#endif // format_hh_INCLUDED
