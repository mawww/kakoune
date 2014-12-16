#ifndef env_vars_hh_INCLUDED
#define env_vars_hh_INCLUDED

#include "unordered_map.hh"

namespace Kakoune
{

class String;
using EnvVarMap = UnorderedMap<String, String>;

EnvVarMap get_env_vars();

}

#endif // env_vars_hh_INCLUDED
