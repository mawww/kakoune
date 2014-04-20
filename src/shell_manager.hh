#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "string.hh"
#include "utils.hh"
#include "env_vars.hh"

namespace Kakoune
{

class Context;
using EnvVarRetriever = std::function<String (StringView name, const Context&)>;

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager();

    String eval(StringView cmdline, const Context& context,
                memoryview<String> params,
                const EnvVarMap& env_vars);

    String pipe(StringView input,
                StringView cmdline, const Context& context,
                memoryview<String> params,
                const EnvVarMap& env_vars);

    void register_env_var(StringView regex, EnvVarRetriever retriever);

private:
    std::vector<std::pair<Regex, EnvVarRetriever>> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED

