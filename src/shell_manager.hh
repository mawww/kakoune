#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "array_view.hh"
#include "env_vars.hh"
#include "string.hh"
#include "utils.hh"
#include "unique_descriptor.hh"
#include "completion.hh"
#include "unique_ptr.hh"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Kakoune
{

class Context;

struct ShellContext
{
    ConstArrayView<String> params;
    EnvVarMap env_vars;
};

struct EnvVarDesc
{
    using Retriever = Vector<String> (*)(StringView name, const Context&);

    StringView str;
    bool prefix;
    Retriever func;
};

inline void closepid(int pid){ kill(pid, SIGTERM); int status = 0; waitpid(pid, &status, 0); }

using UniqueFd = UniqueDescriptor<::close>;
using UniquePid = UniqueDescriptor<closepid>;

struct Shell
{
    UniquePid pid;
    UniqueFd in;
    UniqueFd out;
    UniqueFd err;
};

struct CommandFifos;

class ShellManager : public Singleton<ShellManager>
{
public:
    ShellManager(ConstArrayView<EnvVarDesc> builtin_env_vars);
    ~ShellManager();

    enum class Flags
    {
        None = 0,
        WaitForStdout = 1
    };
    friend constexpr bool with_bit_ops(Meta::Type<Flags>) { return true; }

    std::pair<String, int> eval(StringView cmdline, const Context& context,
                                FunctionRef<StringView ()> stdin_generator,
                                Flags flags = Flags::WaitForStdout,
                                const ShellContext& shell_context = {});

    std::pair<String, int> eval(StringView cmdline, const Context& context,
                                StringView in,
                                Flags flags = Flags::WaitForStdout,
                                const ShellContext& shell_context = {})
    {
        return eval(cmdline, context,
                    [in]() mutable { return std::exchange(in, StringView{}); },
                    flags, shell_context);
    }

    Shell spawn(StringView cmdline,
                const Context& context,
                bool open_stdin,
                const ShellContext& shell_context = {});

    void convert_to_background();
    void remove_background_shell(int pid);
    void clear_removed_background_shell();

    Vector<String> get_val(StringView name, const Context& context) const;

    CandidateList complete_env_var(StringView prefix, ByteCount cursor_pos) const;

private:
    String m_shell;
    bool m_shell_executing = false;
    bool m_convert_to_background_pending = false;

    struct BackgroundShell
    {
        Shell shell;
        UniquePtr<CommandFifos> fifos;
    };
    Vector<BackgroundShell> m_background_shells;

    ConstArrayView<EnvVarDesc> m_env_vars;
};

}

#endif // shell_manager_hh_INCLUDED
