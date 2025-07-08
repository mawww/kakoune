#include "regex.hh"
#include "string_utils.hh"

namespace Kakoune
{

Regex::Regex(StringView re, RegexCompileFlags flags)
    : m_impl{new Impl{}},
      m_str{re.str()}
{
    static_cast<CompiledRegex&>(*m_impl) = compile_regex(re, flags);
}

int Regex::named_capture_index(StringView name) const
{
    for (auto capture : m_impl->named_captures)
    {
        if (capture.name == name)
            return capture.index;
    }
    return -1;
}

String option_to_string(const Regex& re, Quoting quoting)
{
    return option_to_string(re.str(), quoting);
}

Regex option_from_string(Meta::Type<Regex>, StringView str)
{
    return Regex{str};
}

}
