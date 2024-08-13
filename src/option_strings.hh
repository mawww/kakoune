#ifndef option_strings_hh_INCLUDED
#define option_strings_hh_INCLUDED

#include "quoting.hh"
#include "string.hh"
#include "string_utils.hh"
#include "vector.hh"

namespace Kakoune
{

inline String option_to_string(StringView opt, Quoting quoting) { return quoter(quoting)(opt); }
inline Vector<String> option_to_strings(StringView opt) { return {opt.str()}; }
inline String option_from_string(Meta::Type<String>, StringView str) { return str.str(); }
inline bool option_add(String& opt, StringView val) { opt += val; return not val.empty(); }

}

#endif // option_strings_hh_INCLUDED
