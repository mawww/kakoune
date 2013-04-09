#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "string.hh"
#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

struct Context;
typedef std::function<String (const String& name, const Context&)> EnvVarRetriever;
typedef std::unordered_map<String, String>     EnvVarMap;

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager();

    String eval(const String& cmdline, const Context& context,
                const memoryview<String>& params,
                const EnvVarMap& env_vars);

    String pipe(const String& input,
                const String& cmdline, const Context& context,
                const memoryview<String>& params,
                const EnvVarMap& env_vars);

    void register_env_var(const String& regex, EnvVarRetriever retriever);

private:
    Regex                                          m_regex;
    std::vector<std::pair<Regex, EnvVarRetriever>> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED

