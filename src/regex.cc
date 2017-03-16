#include "regex.hh"

#include "exception.hh"

namespace Kakoune
{

using Utf8It = RegexUtf8It<const char*>;

Regex::Regex(StringView re, flag_type flags) try
    : RegexBase{Utf8It{re.begin(), re}, Utf8It{re.end(), re}, flags}, m_str{re.str()}
{} catch (std::runtime_error& err) { throw regex_error(err.what()); }

String option_to_string(const Regex& re)
{
    return re.str();
}

void option_from_string(StringView str, Regex& re)
{
    re = Regex{str};
}

}
