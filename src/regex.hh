#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"
#include "exception.hh"

#ifdef KAK_USE_STDREGEX
#include <regex>
#else
#include <boost/regex.hpp>
#endif

namespace Kakoune
{

struct regex_error : runtime_error
{
    regex_error(StringView desc)
        : runtime_error{format("regex error: '{}'", desc)}
    {}
};

#ifdef KAK_USE_STDREGEX
// Regex that keeps track of its string representation
struct Regex : std::regex
{
    Regex() = default;

    explicit Regex(StringView re, flag_type flags = ECMAScript) try
        : std::regex(re.begin(), re.end(), flags), m_str(re.str()) {}
        catch (std::runtime_error& err) { throw regex_error(err.what()); }

    template<typename Iterator>
    Regex(Iterator begin, Iterator end, flag_type flags = ECMAScript) try
        : std::regex(begin, end, flags), m_str(begin, end) {}
        catch (std::runtime_error& err) { throw regex_error(err.what()); }

    bool empty() const { return m_str.empty(); }
    bool operator==(const Regex& other) const { return m_str == other.m_str; }
    bool operator!=(const Regex& other) const { return m_str != other.m_str; }

    const String& str() const { return m_str; }

private:
    String m_str;
};
namespace regex_ns = std;
#else
struct Regex : boost::regex
{
    Regex() = default;

    explicit Regex(StringView re, flag_type flags = ECMAScript) try
        : boost::regex(re.begin(), re.end(), flags) {}
        catch (std::runtime_error& err) { throw regex_error(err.what()); }

    template<typename Iterator>
    Regex(Iterator begin, Iterator end, flag_type flags = ECMAScript) try
        : boost::regex(begin, end, flags) {}
        catch (std::runtime_error& err) { throw regex_error(err.what()); }

    String str() const { auto s = boost::regex::str(); return {s.begin(), s.end()}; }
};
namespace regex_ns = boost;
#endif

template<typename Iterator>
using RegexIterator = regex_ns::regex_iterator<Iterator>;

template<typename Iterator>
using MatchResults = regex_ns::match_results<Iterator>;

String option_to_string(const Regex& re);
void option_from_string(StringView str, Regex& re);

}

#endif // regex_hh_INCLUDED
