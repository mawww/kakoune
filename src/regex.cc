#include "regex.hh"

#include "buffer_utils.hh"

namespace Kakoune
{

using Utf8It = RegexUtf8It<const char*>;

boost::regbase::flag_type convert_flags(RegexCompileFlags flags)
{
    boost::regbase::flag_type res = boost::regbase::ECMAScript;
    if (flags & RegexCompileFlags::NoSubs)
        res |= boost::regbase::nosubs;
    if (flags & RegexCompileFlags::Optimize)
        res |= boost::regbase::optimize;
    return res;
}

boost::regex_constants::match_flag_type convert_flags(RegexExecFlags flags)
{
    boost::regex_constants::match_flag_type res = boost::regex_constants::match_default;

    if (flags & RegexExecFlags::NotBeginOfLine)
        res |= boost::regex_constants::match_not_bol;
    if (flags & RegexExecFlags::NotEndOfLine)
        res |= boost::regex_constants::match_not_eol;
    if (flags & RegexExecFlags::NotBeginOfWord)
        res |= boost::regex_constants::match_not_bow;
    if (flags & RegexExecFlags::NotEndOfWord)
        res |= boost::regex_constants::match_not_eow;
    if (flags & RegexExecFlags::NotBeginOfSubject)
        res |= boost::regex_constants::match_not_bob;
    if (flags & RegexExecFlags::NotInitialNull)
        res |= boost::regex_constants::match_not_initial_null;
    if (flags & RegexExecFlags::AnyMatch)
        res |= boost::regex_constants::match_any;
    if (flags & RegexExecFlags::PrevAvailable)
        res |= boost::regex_constants::match_prev_avail;

    return res;
}

Regex::Regex(StringView re, RegexCompileFlags flags) try
    : m_impl{new CompiledRegex{compile_regex(re, flags)}},
      m_str{re.str()},
      m_boost_impl{Utf8It{re.begin(), re}, Utf8It{re.end(), re}, convert_flags(flags)}
{
} catch (std::runtime_error& err) { throw regex_error(err.what()); }

String option_to_string(const Regex& re)
{
    return re.str();
}

void option_from_string(StringView str, Regex& re)
{
    re = Regex{str};
}

void regex_mismatch(const Regex& re)
{
    write_to_debug_buffer(format("regex mismatch for '{}'", re.str()));
}

}
