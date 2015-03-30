#ifndef option_types_hh_INCLUDED
#define option_types_hh_INCLUDED

#include "exception.hh"
#include "string.hh"
#include "units.hh"
#include "coord.hh"
#include "array_view.hh"
#include "unordered_map.hh"

#include <tuple>
#include <vector>

namespace Kakoune
{

inline String option_to_string(StringView opt) { return opt.str(); }
inline void option_from_string(StringView str, String& opt) { opt = str.str(); }

inline String option_to_string(int opt) { return to_string(opt); }
inline void option_from_string(StringView str, int& opt) { opt = str_to_int(str); }
inline bool option_add(int& opt, int val) { opt += val; return val != 0; }

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
bool option_add(Vector<T, domain>& opt, const Vector<T, domain>& vec)
{
    std::copy(vec.begin(), vec.end(), back_inserter(opt));
    return not vec.empty();
}

template<typename Key, typename Value, MemoryDomain domain>
String option_to_string(const UnorderedMap<Key, Value, domain>& opt)
{
    String res;
    for (auto it = begin(opt); it != end(opt); ++it)
    {
        if (it != begin(opt))
            res += list_separator;
        String elem = escape(option_to_string(it->first), '=', '\\') + "=" +
                      escape(option_to_string(it->second), '=', '\\');
        res += escape(elem, list_separator, '\\');
    }
    return res;
}

template<typename Key, typename Value, MemoryDomain domain>
void option_from_string(StringView str, UnorderedMap<Key, Value, domain>& opt)
{
    opt.clear();
    for (auto& elem : split(str, list_separator, '\\'))
    {
        Vector<String> pair_str = split(elem, '=', '\\');
        if (pair_str.size() != 2)
            throw runtime_error("map option expects key=value");
        std::pair<Key, Value> pair;
        option_from_string(pair_str[0], pair.first);
        option_from_string(pair_str[1], pair.second);
        opt.insert(std::move(pair));
    }
}

constexpr char tuple_separator = ',';

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
                       StronglyTypedNumber<RealType, ValueType> val)
{
    opt += val;
    return val != 0;
}

template<typename T>
bool option_add(T&, const T&)
{
    throw runtime_error("no add operation supported for this option type");
}

template<typename EffectiveType, typename LineType, typename ColumnType>
inline void option_from_string(StringView str, LineAndColumn<EffectiveType, LineType, ColumnType>& opt)
{
    auto vals = split(str, tuple_separator);
    if (vals.size() != 2)
        throw runtime_error("expected <line>"_str + tuple_separator + "<column>");
    opt.line = str_to_int(vals[0]);
    opt.column = str_to_int(vals[1]);
}

template<typename EffectiveType, typename LineType, typename ColumnType>
inline String option_to_string(const LineAndColumn<EffectiveType, LineType, ColumnType>& opt)
{
    return to_string(opt.line) + tuple_separator + to_string(opt.column);
}

enum YesNoAsk
{
    Yes,
    No,
    Ask
};

inline String option_to_string(YesNoAsk opt)
{
    switch (opt)
    {
        case Yes: return "yes";
        case No:  return "no";
        case Ask: return "ask";
    }
    kak_assert(false);
    return "ask";
}

inline void option_from_string(StringView str, YesNoAsk& opt)
{
    if (str == "yes" or str == "true")
        opt = Yes;
    else if (str == "no" or str == "false")
        opt = No;
    else if (str == "ask")
        opt = Ask;
    else
        throw runtime_error("invalid value '" + str + "', expected yes, no or ask");
}

}

#endif // option_types_hh_INCLUDED
