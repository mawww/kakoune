#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"

#ifdef KAK_USE_STDREGEX
#include <regex>
#else
#include <boost/regex.hpp>
#endif

namespace Kakoune
{

#ifdef KAK_USE_STDREGEX
// Regex that keeps track of its string representation
struct Regex : std::regex
{
    Regex() = default;

    explicit Regex(StringView re, flag_type flags = ECMAScript)
        : std::regex(re.begin(), re.end(), flags), m_str(re) {}

    template<typename Iterator>
    Regex(Iterator begin, Iterator end, flag_type flags = ECMAScript)
        : std::regex(begin, end, flags), m_str(begin, end) {}

    bool empty() const { return m_str.empty(); }
    bool operator==(const Regex& other) { return m_str == other.m_str; }
    bool operator!=(const Regex& other) { return m_str != other.m_str; }

    StringView str() const { return m_str; }

private:
    String m_str;
};
namespace regex_ns = std;
#else
namespace regex_ns = boost;
using Regex = boost::regex;
#endif

template<typename Iterator>
using RegexIterator = regex_ns::regex_iterator<Iterator>;

template<typename Iterator>
using MatchResults = regex_ns::match_results<Iterator>;

using RegexError = regex_ns::regex_error;

String option_to_string(const Regex& re);
void option_from_string(StringView str, Regex& re);

}

#endif // regex_hh_INCLUDED

