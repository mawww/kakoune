#include "regex.hh"

namespace Kakoune
{

Regex::Regex(StringView re, RegexCompileFlags flags)
    : m_impl{new CompiledRegex{compile_regex(re, flags)}},
      m_str{re.str()}
{}

String option_to_string(const Regex& re)
{
    return re.str();
}

Regex option_from_string(Meta::Type<Regex>, StringView str)
{
    return Regex{str};
}

}
