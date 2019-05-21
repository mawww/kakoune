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

struct ShellContext
{
    ConstArrayView<String> params;
    EnvVarMap env_vars;
};

enum class Quoting;

struct EnvVarDesc
{
    using Retriever = String (*)(StringView name, const Context&, Quoting quoting);

    enum class Scopes
    {
        Global = 0x1,
        Buffer = 0x2,
        Window = 0x4
    };
    friend constexpr bool with_bit_ops(Meta::Type<Scopes>) { return true; }

    StringView str;
    bool prefix;
    Scopes scopes;
    Retriever func;
};

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager(ConstArrayView<EnvVarDesc> builtin_env_vars);

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

    String get_val(StringView name, const Context& context, Quoting quoting) const;

    CandidateList complete_env_var(StringView prefix, ByteCount cursor_pos) const;

private:
    String m_shell;

    ConstArrayView<EnvVarDesc> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED
