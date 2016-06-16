#ifndef shell_manager_hh_INCLUDED
#define shell_manager_hh_INCLUDED

#include "array_view.hh"
#include "env_vars.hh"
#include "flags.hh"
#include "string.hh"
#include "utils.hh"
#include "completion.hh"
#include "event_manager.hh"

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

    struct Pipe
    {
        Pipe(bool create = true);
        ~Pipe();

        int read_fd() const { return m_fd[0]; }
        int write_fd() const { return m_fd[1]; }

        void close_read_fd() { close_fd(m_fd[0]); }
        void close_write_fd() { close_fd(m_fd[1]); }

    private:
        void close_fd(int& fd);
        int m_fd[2];
    };

    struct PipeReader : FDWatcher
    {
        PipeReader(Pipe& pipe, String& contents);
    };

    std::pair<String, int> eval(StringView cmdline, const Context& context,
                                StringView input = {},
                                Flags flags = Flags::WaitForStdout,
                                const ShellContext& shell_context = {});

    void register_env_var(StringView str, bool prefix, EnvVarRetriever retriever);
    String get_val(StringView name, const Context& context) const;

    CandidateList complete_env_var(StringView prefix, ByteCount cursor_pos) const;

private:
    struct EnvVarDesc { String str; bool prefix; EnvVarRetriever func; };
    Vector<EnvVarDesc> m_env_vars;
};

template<> struct WithBitOps<ShellManager::Flags> : std::true_type {};

}

#endif // shell_manager_hh_INCLUDED
