#ifndef option_types_hh_INCLUDED
#define option_types_hh_INCLUDED

#include "string.hh"
#include "units.hh"
#include "exception.hh"

#include <tuple>
#include <vector>

namespace Kakoune
{

inline String option_to_string(const String& opt) { return opt; }
inline void option_from_string(const String& str, String& opt) { opt = str; }

inline String option_to_string(int opt) { return int_to_str(opt); }
inline void option_from_string(const String& str, int& opt) { opt = str_to_int(str); }
inline bool option_add(int& opt, int val) { opt += val; return val != 0; }

inline String option_to_string(bool opt) { return opt ? "true" : "false"; }
inline void option_from_string(const String& str, bool& opt)
{
    if (str == "true" or str == "yes")
        opt = true;
    else if (str == "false" or str == "no")
        opt = false;
    else
        throw runtime_error("boolean values are either true, yes, false or no");
}

template<typename T>
String option_to_string(const std::vector<T>& opt)
{
    String res;
    for (size_t i = 0; i < opt.size(); ++i)
    {
        res += option_to_string(opt[i]);
        if (i != opt.size() - 1)
            res += ',';
    }
    return res;
}

template<typename T>
void option_from_string(const String& str, std::vector<T>& opt)
{
    opt.clear();
    std::vector<String> elems = split(str, ',');
    for (auto& elem: elems)
    {
        T opt_elem;
        option_from_string(elem, opt_elem);
        opt.push_back(opt_elem);
    }
}

template<typename T>
bool option_add(std::vector<T>& opt, const std::vector<T>& vec)
{
    std::copy(vec.begin(), vec.end(), back_inserter(opt));
    return not vec.empty();
}


template<size_t I, typename... Types>
struct TupleOptionDetail
{
    static String to_string(const std::tuple<Types...>& opt)
    {
        return TupleOptionDetail<I-1, Types...>::to_string(opt) + ":" + option_to_string(std::get<I>(opt));
    }

    static void from_string(const String& str, std::tuple<Types...>& opt)
    {
        auto it = str.begin();
        auto end = str.end();
        for (size_t i = 0; i < I; ++i)
            it = std::find(it+1, end, ':');
        if (it == end)
            throw runtime_error("not enough elements in tuple");

        option_from_string(String{it+1, std::find(it+1, end, ':')}, std::get<I>(opt));
        TupleOptionDetail<I-1, Types...>::from_string(str, opt);
    }
};

template<typename... Types>
struct TupleOptionDetail<0, Types...>
{
    static String to_string(const std::tuple<Types...>& opt)
    {
        return option_to_string(std::get<0>(opt));
    }

    static void from_string(const String& str, std::tuple<Types...>& opt)
    {
        option_from_string(String{str.begin(), std::find(str.begin(), str.end(), ':')}, std::get<0>(opt));
    }
};

template<typename... Types>
String option_to_string(const std::tuple<Types...>& opt)
{
    return TupleOptionDetail<sizeof...(Types)-1, Types...>::to_string(opt);
}

template<typename... Types>
void option_from_string(const String& str, std::tuple<Types...>& opt)
{
    TupleOptionDetail<sizeof...(Types)-1, Types...>::from_string(str, opt);
}

template<typename RealType, typename ValueType = int>
inline String option_to_string(const StronglyTypedNumber<RealType, ValueType>& opt) { return int_to_str((int)opt); }

template<typename RealType, typename ValueType = int>
inline void option_from_string(const String& str, StronglyTypedNumber<RealType, ValueType>& opt)
{
     opt = StronglyTypedNumber<RealType, ValueType>{str_to_int(str)};
}

template<typename RealType, typename ValueType = int>
inline bool option_add(StronglyTypedNumber<RealType, ValueType>& opt,
                       StronglyTypedNumber<RealType, ValueType> val)
{
    opt += val; return val != 0;
}

template<typename T>
bool option_add(T&, const T&)
{
    throw runtime_error("no add operation supported for this option type");
}

}

#endif // option_types_hh_INCLUDED
