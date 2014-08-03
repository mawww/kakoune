#include "string.hh"

#include "exception.hh"
#include "utils.hh"
#include "utf8_iterator.hh"

namespace Kakoune
{

std::vector<String> split(StringView str, char separator, char escape)
{
    std::vector<String> res;
    auto it = str.begin();
    while (it != str.end())
    {
        res.emplace_back();
        String& element = res.back();
        while (it != str.end())
        {
            auto c = *it;
            if (c == escape and it + 1 != str.end() and *(it+1) == separator)
            {
                element += separator;
                it += 2;
            }
            else if (c == separator)
            {
                ++it;
                break;
            }
            else
            {
                element += c;
                ++it;
            }
        }
    }
    return res;
}

String escape(StringView str, char character, char escape)
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

String escape(StringView str, StringView characters, char escape)
{
    String res;
    for (auto& c : str)
    {
        if (contains(characters, c))
            res += escape;
        res += c;
    }
    return res;
}

int str_to_int(StringView str)
{
    int res = 0;
    if (sscanf(str.zstr(), "%i", &res) != 1)
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

void option_from_string(StringView str, Regex& re)
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

bool prefix_match(StringView str, StringView prefix)
{
    auto it = str.begin();
    for (auto& c : prefix)
    {
        if (it ==str.end() or *it++ != c)
            return false;
    }
    return true;
}

bool subsequence_match(StringView str, StringView subseq)
{
    auto it = str.begin();
    for (auto& c : subseq)
    {
        if (it == str.end())
            return false;
        while (*it != c)
        {
            if (++it == str.end())
                return false;
        }
        ++it;
    }
    return true;
}

String expand_tabs(StringView line, CharCount tabstop, CharCount col)
{
    String res;
    using Utf8It = utf8::iterator<const char*>;
    for (Utf8It it = line.begin(); it.base() < line.end(); ++it)
    {
        if (*it == '\t')
        {
            CharCount end_col = (col / tabstop + 1) * tabstop;
            res += String{' ', end_col - col};
        }
        else
            res += *it;
    }
    return res;
}

}
