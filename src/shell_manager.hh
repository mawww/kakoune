#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "array_view.hh"
#include "regex.hh"
#include "utils.hh"
#include "env_vars.hh"

namespace Kakoune
{

class Context;
class String;
class StringView;

using EnvVarRetriever = std::function<String (StringView name, const Context&)>;

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager();

    std::pair<String, int> eval(StringView cmdline, const Context& context,
                                StringView input = {},
                                ConstArrayView<String> params = {},
                                const EnvVarMap& env_vars = EnvVarMap{});

    void register_env_var(StringView regex, EnvVarRetriever retriever);
    String get_val(StringView name, const Context& context) const;

private:
    Vector<std::pair<Regex, EnvVarRetriever>> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED
