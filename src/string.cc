#include "string.hh"

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

std::vector<String> split(const String& str, Character separator)
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

String String::replace(const String& expression,
                       const String& replacement) const
{
   boost::regex re(expression.m_content);
   return String(boost::regex_replace(m_content, re, replacement.m_content));
}

}
