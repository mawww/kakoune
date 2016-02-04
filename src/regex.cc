#include "regex.hh"

#include "exception.hh"

namespace Kakoune
{

String option_to_string(const Regex& re)
{
    return re.str();
}

void option_from_string(StringView str, Regex& re)
{
    re = Regex{str};
}

}
