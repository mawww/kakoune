#include "string.hh"

#include "exception.hh"

namespace Kakoune
{

std::vector<String> split(const String& str, char separator, char escape)
{
    auto begin = str.begin();
    auto end   = str.begin();

    std::vector<String> res;
    while (end != str.end())
    {
        res.emplace_back();
        String& element = res.back();
        while (end != str.end())
        {
            auto c = *end;
            if (c == escape and end + 1 != end and *(end+1) == separator)
            {
                element += separator;
                end += 2;
            }
            else if (c == separator)
            {
                ++end;
                break;
            }
            else
            {
                element += c;
                ++end;
            }
        }
        begin = end;
    }
    return res;
}

String escape(const String& str, char character, char escape)
{
    String res;
    for (auto& c : str)
    {
        if (c == character)
            res += escape;
        res += c;
    }
    return res;
}

int str_to_int(const String& str)
{
    int res = 0;
    if (sscanf(str.c_str(), "%i", &res) != 1)
        throw runtime_error(str + "is not a number");
    return res;
}

String to_string(int val)
{
    char buf[16];
    sprintf(buf, "%i", val);
    return buf;
}

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

}
