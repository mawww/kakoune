#include "regex.hh"
#include "ranges.hh"
#include "string_utils.hh"

namespace Kakoune
{

Regex::Regex(StringView re, RegexCompileFlags flags)
    : m_impl{new CompiledRegex{}},
      m_str{re.str()}
{
    *m_impl = compile_regex(re, flags);
}

int Regex::named_capture_index(StringView name) const
{
    auto it = find_if(m_impl->named_captures, [&](auto& c) { return c.name == name; });
    return it != m_impl->named_captures.end() ? it->index : -1;
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
