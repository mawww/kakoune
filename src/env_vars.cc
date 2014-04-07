#include "env_vars.hh"

namespace Kakoune
{

EnvVarMap get_env_vars()
{
    EnvVarMap env_vars;
    for (char** it = environ; *it; ++it)
    {
        const char* name = *it;
        const char* value = name;
        while (*value != 0 and *value != '=')
            ++value;
        env_vars[String{name, value}] = (*value == '=') ? value+1 : value;
    }
    return env_vars;
}

}
