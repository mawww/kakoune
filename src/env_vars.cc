#include "env_vars.hh"

#if __APPLE__
extern char **environ;
#endif

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
