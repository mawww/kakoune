#include "regex.hh"

#include "exception.hh"

namespace Kakoune
{

String option_to_string(const Regex& re)
{
    const auto& str = re.str();
    return {str.begin(), str.end()};
}

void option_from_string(StringView str, Regex& re)
{
    try
    {
        re = Regex{str.begin(), str.end()};
    }
    catch (RegexError& err)
    {
        throw runtime_error("unable to create regex: "_str + err.what());
    }
}

}
