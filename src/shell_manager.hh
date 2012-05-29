#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "utils.hh"
#include "regex.hh"

#include <unordered_map>

namespace Kakoune
{

class Context;
typedef std::function<String (const Context&)> EnvVarRetriever;
typedef std::unordered_map<String, String>     EnvVarMap;

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager();

    String eval(const String& cmdline, const Context& context,
                const EnvVarMap& env_vars);

    void register_env_var(const String& name, EnvVarRetriever retriever);

private:
    Regex                                       m_regex;
    std::unordered_map<String, EnvVarRetriever> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED

