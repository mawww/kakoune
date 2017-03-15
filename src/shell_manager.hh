#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "array_view.hh"
#include "env_vars.hh"
#include "string.hh"
#include "utils.hh"
#include "completion.hh"

namespace Kakoune
{

class Context;

using EnvVarRetriever = std::function<String (StringView name, const Context&)>;

struct ShellContext
{
    ConstArrayView<String> params;
    EnvVarMap env_vars;
};

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager();

    enum class Flags
    {
        None = 0,
        WaitForStdout = 1
    };
    friend constexpr bool with_bit_ops(Meta::Type<Flags>) { return true; }

    std::pair<String, int> eval(StringView cmdline, const Context& context,
                                StringView input = {},
                                Flags flags = Flags::WaitForStdout,
                                const ShellContext& shell_context = {});

    void register_env_var(StringView str, bool prefix, EnvVarRetriever retriever);
    String get_val(StringView name, const Context& context) const;

    CandidateList complete_env_var(StringView prefix, ByteCount cursor_pos) const;

private:
    String m_shell;

    struct EnvVarDesc { String str; bool prefix; EnvVarRetriever func; };
    Vector<EnvVarDesc, MemoryDomain::EnvVars> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED
