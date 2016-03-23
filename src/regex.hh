#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"
#include "exception.hh"

#ifdef KAK_USE_STDREGEX
#include <regex>
#else
#include <boost/regex/icu.hpp>
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

template<typename Iterator>
using RegexIterator = regex_ns::regex_iterator<Iterator>;

template <typename... Args>
auto regex_match(Args&&... args) -> decltype(std::regex_match(std::forward<Args>(args)...)) {
  return std::regex_match(std::forward<Args>(args)...);
}
template <typename... Args>
auto regex_search(Args&&... args) -> decltype(std::regex_search(std::forward<Args>(args)...)) {
  return std::regex_search(std::forward<Args>(args)...);
}
#else

struct Regex : boost::u32regex
{
    Regex() = default;

    explicit Regex(StringView re, flag_type flags = ECMAScript) try
        : boost::u32regex(boost::make_u32regex(re.begin(), re.end(), flags)) {}
        catch (std::runtime_error& err) { throw regex_error(err.what()); }

    template<typename Iterator>
    Regex(Iterator begin, Iterator end, flag_type flags = ECMAScript) try
        : boost::u32regex(boost::make_u32regex(begin, end, flags)) {}
        catch (std::runtime_error& err) { throw regex_error(err.what()); }

    String str() const { String res; for (auto cp : boost::u32regex::str()) utf8::dump(std::back_inserter(res), cp); return res; }
};
namespace regex_ns = boost;

template<typename Iterator>
using RegexIterator = regex_ns::u32regex_iterator<Iterator>;

template <typename... Args>
auto regex_match(Args&&... args) -> decltype(boost::u32regex_match(std::forward<Args>(args)...)) {
  return boost::u32regex_match(std::forward<Args>(args)...);
}
template <typename... Args>
auto regex_search(Args&&... args) -> decltype(boost::u32regex_search(std::forward<Args>(args)...)) {
  return boost::u32regex_search(std::forward<Args>(args)...);
}
#endif

template<typename Iterator>
using MatchResults = regex_ns::match_results<Iterator>;

namespace RegexConstant = regex_ns::regex_constants;

inline RegexConstant::match_flag_type match_flags(bool bol, bool eol, bool eow)
{
    return (bol ? RegexConstant::match_default : RegexConstant::match_not_bol |
                                                 RegexConstant::match_prev_avail) |
           (eol ? RegexConstant::match_default : RegexConstant::match_not_eol) |
           (eow ? RegexConstant::match_default : RegexConstant::match_not_eow);
}

String option_to_string(const Regex& re);
void option_from_string(StringView str, Regex& re);

}

#endif // regex_hh_INCLUDED
