#ifndef option_types_hh_INCLUDED
#define option_types_hh_INCLUDED

#include "exception.hh"
#include "string.hh"
#include "units.hh"
#include "coord.hh"
#include "array_view.hh"
#include "id_map.hh"
#include "flags.hh"
#include "enum.hh"

#include <tuple>
#include <vector>

namespace Kakoune
{

inline String option_to_string(StringView opt) { return opt.str(); }
inline void option_from_string(StringView str, String& opt) { opt = str.str(); }
inline bool option_add(String& opt, StringView val) { opt += val; return not val.empty(); }

inline String option_to_string(int opt) { return to_string(opt); }
inline void option_from_string(StringView str, int& opt) { opt = str_to_int(str); }
inline bool option_add(int& opt, StringView str)
{
    auto val = str_to_int(str);
    opt += val;
    return val != 0;
}

inline String option_to_string(bool opt) { return opt ? "true" : "false"; }
inline void option_from_string(StringView str, bool& opt)
{
    if (str == "true" or str == "yes")
        opt = true;
    else if (str == "false" or str == "no")
        opt = false;
    else
        throw runtime_error("boolean values are either true, yes, false or no");
}

constexpr char list_separator = ':';

template<typename T, MemoryDomain domain>
String option_to_string(const Vector<T, domain>& opt)
{
    String res;
    for (size_t i = 0; i < opt.size(); ++i)
    {
        res += escape(option_to_string(opt[i]), list_separator, '\\');
        if (i != opt.size() - 1)
            res += list_separator;
    }
    return res;
}

template<typename T, MemoryDomain domain>
void option_from_string(StringView str, Vector<T, domain>& opt)
{
    opt.clear();
    Vector<String> elems = split(str, list_separator, '\\');
    for (auto& elem: elems)
    {
        T opt_elem;
        option_from_string(elem, opt_elem);
        opt.push_back(opt_elem);
    }
}

template<typename T, MemoryDomain domain>
bool option_add(Vector<T, domain>& opt, StringView str)
{
    Vector<T, domain> vec;
    option_from_string(str, vec);
    std::copy(std::make_move_iterator(vec.begin()),
              std::make_move_iterator(vec.end()),
              back_inserter(opt));
    return not vec.empty();
}

template<typename Value, MemoryDomain domain>
String option_to_string(const IdMap<Value, domain>& opt)
{
    String res;
    for (auto it = begin(opt); it != end(opt); ++it)
    {
        if (it != begin(opt))
            res += list_separator;
        String elem = escape(option_to_string(it->key), '=', '\\') + "=" +
                      escape(option_to_string(it->value), '=', '\\');
        res += escape(elem, list_separator, '\\');
    }
    return res;
}

template<typename Value, MemoryDomain domain>
void option_from_string(StringView str, IdMap<Value, domain>& opt)
{
    opt.clear();
    for (auto& elem : split(str, list_separator, '\\'))
    {
        Vector<String> pair_str = split(elem, '=', '\\');
        if (pair_str.size() != 2)
            throw runtime_error("map option expects key=value");
        String key;
        Value value;
        option_from_string(pair_str[0], key);
        option_from_string(pair_str[1], value);
        opt.append({ std::move(key), std::move(value) });
    }
}

constexpr char tuple_separator = '|';

template<size_t I, typename... Types>
struct TupleOptionDetail
{
    static String to_string(const std::tuple<Types...>& opt)
    {
        return TupleOptionDetail<I-1, Types...>::to_string(opt) +
               tuple_separator + escape(option_to_string(std::get<I>(opt)), tuple_separator, '\\');
    }

    static void from_string(ConstArrayView<String> elems, std::tuple<Types...>& opt)
    {
        option_from_string(elems[I], std::get<I>(opt));
        TupleOptionDetail<I-1, Types...>::from_string(elems, opt);
    }
};

template<typename... Types>
struct TupleOptionDetail<0, Types...>
{
    static String to_string(const std::tuple<Types...>& opt)
    {
        return option_to_string(std::get<0>(opt));
    }

    static void from_string(ConstArrayView<String> elems, std::tuple<Types...>& opt)
    {
        option_from_string(elems[0], std::get<0>(opt));
    }
};

template<typename... Types>
String option_to_string(const std::tuple<Types...>& opt)
{
    return TupleOptionDetail<sizeof...(Types)-1, Types...>::to_string(opt);
}

template<typename... Types>
void option_from_string(StringView str, std::tuple<Types...>& opt)
{
    auto elems = split(str, tuple_separator, '\\');
    if (elems.size() != sizeof...(Types))
        throw runtime_error(elems.size() < sizeof...(Types) ?
                              "not enough elements in tuple"
                            : "to many elements in tuple");
    TupleOptionDetail<sizeof...(Types)-1, Types...>::from_string(elems, opt);
}

template<typename RealType, typename ValueType>
inline String option_to_string(const StronglyTypedNumber<RealType, ValueType>& opt)
{
    return to_string(opt);
}

template<typename RealType, typename ValueType>
inline void option_from_string(StringView str, StronglyTypedNumber<RealType, ValueType>& opt)
{
     opt = StronglyTypedNumber<RealType, ValueType>{str_to_int(str)};
}

template<typename RealType, typename ValueType>
inline bool option_add(StronglyTypedNumber<RealType, ValueType>& opt,
                       StringView str)
{
    int val = str_to_int(str);
    opt += val;
    return val != 0;
}

template<typename T>
bool option_add(T&, StringView str)
{
    throw runtime_error("no add operation supported for this option type");
}

template<typename EffectiveType, typename LineType, typename ColumnType>
inline void option_from_string(StringView str, LineAndColumn<EffectiveType, LineType, ColumnType>& opt)
{
    auto vals = split(str, ',');
    if (vals.size() != 2)
        throw runtime_error("expected <line>,<column>");
    opt.line = str_to_int(vals[0]);
    opt.column = str_to_int(vals[1]);
}

template<typename EffectiveType, typename LineType, typename ColumnType>
inline String option_to_string(const LineAndColumn<EffectiveType, LineType, ColumnType>& opt)
{
    return format("{},{}", opt.line, opt.column);
}

enum class DebugFlags
{
    None  = 0,
    Hooks = 1 << 0,
    Shell = 1 << 1,
    Profile = 1 << 2,
};

template<>
struct WithBitOps<DebugFlags> : std::true_type {};

constexpr Array<EnumDesc<DebugFlags>, 3> enum_desc(DebugFlags)
{
    return { {
        { DebugFlags::Hooks, "hooks" },
        { DebugFlags::Shell, "shell" },
        { DebugFlags::Profile, "profile" }
    } };
}

template<typename T>
struct TimestampedList
{
    size_t timestamp;
    Vector<T, MemoryDomain::Options> list;
};

template<typename T>
inline bool operator==(const TimestampedList<T>& lhs, const TimestampedList<T>& rhs)
{
    return lhs.timestamp == rhs.timestamp and lhs.list == rhs.list;
}

template<typename T>
inline bool operator!=(const TimestampedList<T>& lhs, const TimestampedList<T>& rhs)
{
    return not (lhs == rhs);
}

template<typename T>
inline String option_to_string(const TimestampedList<T>& opt)
{
    return format("{}:{}", opt.timestamp, option_to_string(opt.list));
}

template<typename T>
inline void option_from_string(StringView str, TimestampedList<T>& opt)
{
    auto it = find(str, ':');
    opt.timestamp = str_to_int({str.begin(), it});
    if (it != str.end())
        option_from_string({it+1, str.end()}, opt.list);
}

template<typename T>
inline bool option_add(TimestampedList<T>& opt, StringView str)
{
    return option_add(opt.list, str);
}

}

#endif // option_types_hh_INCLUDED
