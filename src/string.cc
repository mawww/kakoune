#include "string.hh"

#include "exception.hh"

namespace Kakoune
{

String int_to_str(int value)
{
    const bool negative = value < 0;
    if (negative)
        value = -value;

    char buffer[16];
    size_t pos = sizeof(buffer);
    buffer[--pos] = 0;
    do
    {
        buffer[--pos] = '0' + (value % 10);
        value /= 10;
    }
    while (value);

    if (negative)
       buffer[--pos] = '-';

    return String(buffer + pos);
}

int str_to_int(const String& str)
{
    return atoi(str.c_str());
}

std::vector<String> split(const String& str, char separator)
{
    auto begin = str.begin();
    auto end   = str.begin();

    std::vector<String> res;
    while (end != str.end())
    {
        while (end != str.end() and *end != separator)
            ++end;
        res.push_back(String(begin, end));
        if (end == str.end())
            break;
        begin = ++end;
    }
    return res;
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
