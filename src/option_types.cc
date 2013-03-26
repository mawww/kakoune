#include "option_types.hh"

#include "exception.hh"

namespace Kakoune
{

String option_to_string(const Regex& re)
{
    return String{re.str()};
}

void option_from_string(const String& str, Regex& re)
{
    try
    {
        re = Regex{str.begin(), str.end()};
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error("unable to create regex: "_str + err.what());
    }
}

String option_to_string(const LineAndFlag& opt)
{
    return int_to_str((int)opt.line) + ":" + color_to_str(opt.color) + ":" + opt.flag;
}

void option_from_string(const String& str, LineAndFlag& opt)
{
    static Regex re{R"((\d+):(\w+):(.+))"};

    boost::match_results<String::iterator> res;
    if (not boost::regex_match(str.begin(), str.end(), res, re))
        throw runtime_error("wrong syntax, expected <line>:<color>:<flag>");

    opt.line = str_to_int(String{res[1].first, res[1].second});
    opt.color = str_to_color(String{res[2].first, res[2].second});
    opt.flag = String{res[3].first, res[3].second};
}

}
