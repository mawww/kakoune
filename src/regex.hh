#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"

#include <boost/regex.hpp>

namespace Kakoune
{

typedef boost::basic_regex<Character> Regex;

}

#endif // regex_hh_INCLUDED

