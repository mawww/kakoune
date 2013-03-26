#ifndef option_types_hh_INCLUDED
#define option_types_hh_INCLUDED

#include "units.hh"
#include "string.hh"
#include "color.hh"
#include "exception.hh"

namespace Kakoune
{

inline String option_to_string(const String& opt) { return opt; }
inline void option_from_string(const String& str, String& opt) { opt = str; }

inline String option_to_string(int opt) { return int_to_str(opt); }
inline void option_from_string(const String& str, int& opt) { opt = str_to_int(str); }

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

String option_to_string(const Regex& re);
void option_from_string(const String& str, Regex& re);

struct LineAndFlag
{
    LineCount line;
    Color     color;
    String    flag;

    bool operator==(const LineAndFlag& other) const
    { return line == other.line and color == other.color and flag == other.flag; }

    bool operator!=(const LineAndFlag& other) const
    { return not (*this == other); }
};

String option_to_string(const LineAndFlag& opt);
void option_from_string(const String& str, LineAndFlag& opt);

}

#endif // option_types_hh_INCLUDED
