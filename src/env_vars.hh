#ifndef env_vars_hh_INCLUDED
#define env_vars_hh_INCLUDED

#include "string.hh"

#include <unordered_map>

namespace Kakoune
{

using EnvVarMap = std::unordered_map<String, String>;

EnvVarMap get_env_vars();

}

#endif // env_vars_hh_INCLUDED

