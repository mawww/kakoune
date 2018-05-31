#ifndef option_types_hh_INCLUDED
#define option_types_hh_INCLUDED

#include "array_view.hh"
#include "coord.hh"
#include "exception.hh"
#include "flags.hh"
#include "hash_map.hh"
#include "option.hh"
#include "ranges.hh"
#include "string.hh"
#include "string_utils.hh"
#include "units.hh"

#include <tuple>
#include <vector>

namespace Kakoune
{

template<typename T>
constexpr decltype(T::option_type_name) option_type_name(Meta::Type<T>)
{
    return T::option_type_name;
}

template<typename Enum>
std::enable_if_t<std::is_enum<Enum>::value, String>
option_type_name(Meta::Type<Enum>)
{
    return format("{}({})", with_bit_ops(Meta::Type<Enum>{}) ? "flags" : "enum",
                  join(enum_desc(Meta::Type<Enum>{}) |
                       transform(&EnumDesc<Enum>::name), '|'));
}

inline String option_to_string(int opt) { return to_string(opt); }
inline int option_from_string(Meta::Type<int>, StringView str) { return str_to_int(str); }
inline bool option_add(int& opt, StringView str)
{
    auto val = str_to_int(str);
    opt += val;
    return val != 0;
}
constexpr StringView option_type_name(Meta::Type<int>) { return "int"; }

inline String option_to_string(size_t opt) { return to_string(opt); }
inline size_t option_from_string(Meta::Type<size_t>, StringView str) { return str_to_int(str); }

inline String option_to_string(bool opt) { return opt ? "true" : "false"; }
inline bool option_from_string(Meta::Type<bool>, StringView str)
{
    if (str == "true" or str == "yes")
        return true;
    else if (str == "false" or str == "no")
        return false;
    else
        throw runtime_error("boolean values are either true, yes, false or no");
}
constexpr StringView option_type_name(Meta::Type<bool>) { return "bool"; }

inline String option_to_string(Codepoint opt) { return to_string(opt); }
inline Codepoint option_from_string(Meta::Type<Codepoint>, StringView str)
{
    if (str.char_length() != 1)
        throw runtime_error{format("'{}' is not a single codepoint", str)};
    return str[0_char];
}
constexpr StringView option_type_name(Meta::Type<Codepoint>) { return "codepoint"; }

template<typename T, MemoryDomain domain>
Vector<String> option_to_strings(const Vector<T, domain>& opt)
{
    return opt | transform([](const T& t) { return option_to_string(t); }) | gather<Vector<String>>();
}

template<typename T, MemoryDomain domain>
String option_to_string(const Vector<T, domain>& opt)
{
    return join(opt | transform([](const T& t) { return quote(option_to_string(t)); }), ' ', false);
}

template<typename T, MemoryDomain domain>
void option_list_postprocess(Vector<T, domain>& opt)
{}

template<typename T, MemoryDomain domain>
Vector<T, domain> option_from_strings(Meta::Type<Vector<T, domain>>, ConstArrayView<String> strs)
{
    auto res =  strs | transform([](auto&& s) { return option_from_string(Meta::Type<T>{}, s); })
                     | gather<Vector<T, domain>>();
    option_list_postprocess(res);
    return res;
}

template<typename T, MemoryDomain domain>
bool option_add_from_strings(Vector<T, domain>& opt, ConstArrayView<String> strs)
{
    auto vec = option_from_strings(Meta::Type<Vector<T, domain>>{}, strs);
    opt.insert(opt.end(),
               std::make_move_iterator(vec.begin()),
               std::make_move_iterator(vec.end()));
    option_list_postprocess(opt);
    return not vec.empty();
}

template<typename T, MemoryDomain D>
String option_type_name(Meta::Type<Vector<T, D>>)
{
    return option_type_name(Meta::Type<T>{}) + "-list"_sv;
}

template<typename Key, typename Value, MemoryDomain domain>
Vector<String> option_to_strings(const HashMap<Key, Value, domain>& opt)
{
    return opt | transform([](auto&& item) {
        return format("{}={}",
                      escape(option_to_string(item.key), '=', '\\'),
                      escape(option_to_string(item.value), '=', '\\'));
    }) | gather<Vector<String>>();
}

template<typename Key, typename Value, MemoryDomain domain>
String option_to_string(const HashMap<Key, Value, domain>& opt)
{
    return join(opt | transform([](auto&& item) {
        return quote(format("{}={}",
                      escape(option_to_string(item.key), '=', '\\'),
                      escape(option_to_string(item.value), '=', '\\')));
    }), ' ', false);
}

template<typename Key, typename Value, MemoryDomain domain>
bool option_add_from_strings(HashMap<Key, Value, domain>& opt, ConstArrayView<String> strs)
{
    bool changed = false;
    for (auto&& str : strs)
    {
        struct error : runtime_error { error(size_t) : runtime_error{"map option expects key=value"} {} };
        auto key_value = str | split<StringView>('=', '\\')
                             | transform(unescape<'=', '\\'>)
                             | static_gather<error, 2>();

        opt[option_from_string(Meta::Type<Key>{}, key_value[0])] = option_from_string(Meta::Type<Value>{}, key_value[1]);
        changed = true;
    }
    return changed;
}

template<typename Key, typename Value, MemoryDomain domain>
HashMap<Key, Value, domain> option_from_strings(Meta::Type<HashMap<Key, Value, domain>>, ConstArrayView<String> str)
{
    HashMap<Key, Value, domain> res;
    option_add_from_strings(res, str);
    return res;
}

template<typename K, typename V, MemoryDomain D>
String option_type_name(Meta::Type<HashMap<K, V, D>>)
{
    return format("{}-to-{}-map", option_type_name(Meta::Type<K>{}),
                  option_type_name(Meta::Type<V>{}));
}

constexpr char tuple_separator = '|';

template<typename... Types, size_t... I>
String option_to_string_impl(const std::tuple<Types...>& opt, std::index_sequence<I...>)
{
    return join(make_array({option_to_string(std::get<I>(opt))...}), tuple_separator);
}

template<typename... Types>
String option_to_string(const std::tuple<Types...>& opt)
{
    return option_to_string_impl(opt, std::make_index_sequence<sizeof...(Types)>());
}

template<typename... Types, size_t... I>
std::tuple<Types...> option_from_string_impl(Meta::Type<std::tuple<Types...>>, StringView str,
                                             std::index_sequence<I...>)
{
    struct error : runtime_error
    {
        error(size_t i) : runtime_error{i < sizeof...(Types) ?
                                          "not enough elements in tuple"
                                        : "too many elements in tuple"} {}
    };
    auto elems = str | split<StringView>(tuple_separator, '\\')
                     | transform(unescape<tuple_separator, '\\'>)
                     | static_gather<error, sizeof...(Types)>();
    return std::tuple<Types...>{option_from_string(Meta::Type<Types>{}, elems[I])...};
}

template<typename... Types>
std::tuple<Types...> option_from_string(Meta::Type<std::tuple<Types...>>, StringView str)
{
    return option_from_string_impl(Meta::Type<std::tuple<Types...>>{}, str,
                                   std::make_index_sequence<sizeof...(Types)>());
}

template<typename RealType, typename ValueType>
inline String option_to_string(const StronglyTypedNumber<RealType, ValueType>& opt)
{
    return to_string(opt);
}

template<typename Number>
std::enable_if_t<std::is_base_of<StronglyTypedNumber<Number, int>, Number>::value, Number>
option_from_string(Meta::Type<Number>, StringView str)
{
     return Number{str_to_int(str)};
}

template<typename RealType, typename ValueType>
inline bool option_add(StronglyTypedNumber<RealType, ValueType>& opt, StringView str)
{
    int val = str_to_int(str);
    opt += val;
    return val != 0;
}

struct WorstMatch { template<typename T> WorstMatch(T&&) {} };

inline bool option_add(WorstMatch, StringView)
{
    throw runtime_error("no add operation supported for this option type");
}

class Context;

inline void option_update(WorstMatch, const Context&)
{
    throw runtime_error("no update operation supported for this option type");
}

template<typename Coord>
std::enable_if_t<std::is_base_of<LineAndColumn<Coord, decltype(Coord::line), decltype(Coord::column)>, Coord>::value, Coord>
option_from_string(Meta::Type<Coord>, StringView str)
{
    struct error : runtime_error { error(size_t) : runtime_error{"expected <line>,<column>"} {} };
    auto vals = str | split<StringView>(',')
                    | static_gather<error, 2>();
    return {str_to_int(vals[0]), str_to_int(vals[1])};
}

template<typename EffectiveType, typename LineType, typename ColumnType>
inline String option_to_string(const LineAndColumn<EffectiveType, LineType, ColumnType>& opt)
{
    return format("{},{}", opt.line, opt.column);
}

template<typename Flags, typename = decltype(enum_desc(Meta::Type<Flags>{}))>
EnableIfWithBitOps<Flags, String> option_to_string(Flags flags)
{
    constexpr auto desc = enum_desc(Meta::Type<Flags>{});
    String res;
    for (int i = 0; i < desc.size(); ++i)
    {
        if (not (flags & desc[i].value))
            continue;
        if (not res.empty())
            res += "|";
        res += desc[i].name;
    }
    return res;
}

template<typename Enum, typename = decltype(enum_desc(Meta::Type<Enum>{}))>
EnableIfWithoutBitOps<Enum, String> option_to_string(Enum e)
{
    constexpr auto desc = enum_desc(Meta::Type<Enum>{});
    auto it = find_if(desc, [e](const EnumDesc<Enum>& d) { return d.value == e; });
    if (it != desc.end())
        return it->name.str();
    kak_assert(false);
    return {};
}

template<typename Flags, typename = decltype(enum_desc(Meta::Type<Flags>{}))>
EnableIfWithBitOps<Flags, Flags> option_from_string(Meta::Type<Flags>, StringView str)
{
    constexpr auto desc = enum_desc(Meta::Type<Flags>{});
    Flags flags{};
    for (auto s : str | split<StringView>('|'))
    {
        auto it = find_if(desc, [s](const EnumDesc<Flags>& d) { return d.name == s; });
        if (it == desc.end())
            throw runtime_error(format("invalid flag value '{}'", s));
        flags |= it->value;
    }
    return flags;
}

template<typename Enum, typename = decltype(enum_desc(Meta::Type<Enum>{}))>
EnableIfWithoutBitOps<Enum, Enum> option_from_string(Meta::Type<Enum>, StringView str)
{
    constexpr auto desc = enum_desc(Meta::Type<Enum>{});
    auto it = find_if(desc, [str](const EnumDesc<Enum>& d) { return d.name == str; });
    if (it == desc.end())
        throw runtime_error(format("invalid enum value '{}'", str));
    return it->value;
}

template<typename Flags, typename = decltype(enum_desc(Meta::Type<Flags>{}))>
EnableIfWithBitOps<Flags, bool> option_add(Flags& opt, StringView str)
{
    const Flags res = option_from_string(Meta::Type<Flags>{}, str);
    opt |= res;
    return res != (Flags)0;
}

template<typename P, typename T>
inline Vector<String> option_to_strings(const PrefixedList<P, T>& opt)
{
    Vector<String> res{option_to_string(opt.prefix)};
    auto list = option_to_strings(opt.list);
    res.insert(res.end(), std::make_move_iterator(list.begin()), std::make_move_iterator(list.end()));
    return res;
}

template<typename P, typename T>
inline String option_to_string(const PrefixedList<P, T>& opt)
{
    return option_to_string(opt.prefix) + " " + option_to_string(opt.list);
}

template<typename P, typename T>
inline PrefixedList<P, T> option_from_strings(Meta::Type<PrefixedList<P, T>>, ConstArrayView<String> strs)
{
    return {option_from_string(Meta::Type<P>{}, strs[0]), 
            option_from_strings(Meta::Type<Vector<T, MemoryDomain::Options>>{}, strs.subrange(1))};
}

template<typename P, typename T>
inline bool option_add_from_strings(PrefixedList<P, T>& opt, ConstArrayView<String> str)
{
    return option_add_from_strings(opt.list, str);
}

}

#endif // option_types_hh_INCLUDED
