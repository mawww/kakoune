#ifndef env_vars_hh_INCLUDED
#define env_vars_hh_INCLUDED

#include "hash_map.hh"

namespace Kakoune
{

class String;
using EnvVarMap = HashMap<String, String, MemoryDomain::EnvVars>;

EnvVarMap get_env_vars();

}

#endif // env_vars_hh_INCLUDED
