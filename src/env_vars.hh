#pragma once

#include "hash_map.hh"

namespace Kakoune
{

class String;
using EnvVarMap = HashMap<String, String, MemoryDomain::EnvVars>;

EnvVarMap get_env_vars();

}
