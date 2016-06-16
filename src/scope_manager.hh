#ifndef scope_manager_hh_INCLUDED
#define scope_manager_hh_INCLUDED

#include "shell_manager.hh"

namespace Kakoune
{

class ScopeManager : public Singleton<ScopeManager>
{
public:
    ScopeManager();

    std::pair<String, int> eval(char const *prog_binary,
                                StringView cmdline,
                                const Context& context,
                                ShellManager::Flags flags = ShellManager::Flags::WaitForStdout,
                                const ShellContext& shell_context = {});

    void register_env_var(StringView str, bool prefix, EnvVarRetriever retriever);
    String get_val(StringView name, const Context& context) const;

    CandidateList complete_env_var(StringView prefix, ByteCount cursor_pos) const;

private:
    struct EnvVarDesc { String str; bool prefix; EnvVarRetriever func; };
    Vector<EnvVarDesc> m_env_vars;
};

}

#endif // scope_manager_hh_INCLUDED
