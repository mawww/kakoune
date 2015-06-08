#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "array_view.hh"
#include "regex.hh"
#include "utils.hh"
#include "env_vars.hh"
#include "flags.hh"

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

    enum class Flags
    {
        None = 0,
        WaitForStdout = 1
    };

    std::pair<String, int> eval(StringView cmdline, const Context& context,
                                StringView input = {},
                                Flags flags = Flags::WaitForStdout,
                                ConstArrayView<String> params = {},
                                const EnvVarMap& env_vars = EnvVarMap{});

    void register_env_var(StringView regex, EnvVarRetriever retriever);
    String get_val(StringView name, const Context& context) const;

private:
    Vector<std::pair<Regex, EnvVarRetriever>> m_env_vars;
};

template<> struct WithBitOps<ShellManager::Flags> : std::true_type {};

}

#endif // shell_manager_hh_INCLUDED
