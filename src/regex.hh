#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"

#if 0
#include <regex>
namespace kak_regex_ns = std;
#else
#include <boost/regex.hpp>
namespace kak_regex_ns = boost;
#endif

namespace Kakoune
{

using Regex = kak_regex_ns::regex;

template<typename Iterator>
using RegexIterator = kak_regex_ns::regex_iterator<Iterator>;

template<typename Iterator>
using MatchResults = kak_regex_ns::match_results<Iterator>;

using RegexError = kak_regex_ns::regex_error;

String option_to_string(const Regex& re);
void option_from_string(StringView str, Regex& re);

}

#endif // regex_hh_INCLUDED

