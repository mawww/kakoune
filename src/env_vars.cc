#include "env_vars.hh"

#include "string.hh"

extern char **environ;

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
        env_vars.insert({String{String::NoCopy{}, {name, value}},
                         (*value == '=') ? String{String::NoCopy{}, value+1} : String{}});
    }
    return env_vars;
}

}
