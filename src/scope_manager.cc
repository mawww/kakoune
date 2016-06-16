#include "scope_manager.hh"

#include "context.hh"
#include "buffer_utils.hh"
#include "event_manager.hh"
#include "file.hh"

#include <chrono>

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

extern char **environ;

namespace Kakoune
{

ScopeManager::ScopeManager()
{
    const char* path = getenv("PATH");
    auto new_path = format("{}:{}", path, split_path(get_kak_binary_path()).first);
    setenv("PATH", new_path.c_str(), 1);
}

namespace
{

template<typename Func>
static pid_t spawn_interpreter(String prog_path, ConstArrayView<String> params,
                        ConstArrayView<String> kak_env, Func setup_child)
{
    Vector<const char*> envptrs;
    for (char** envp = environ; *envp; ++envp)
        envptrs.push_back(*envp);
    for (auto& env : kak_env)
        envptrs.push_back(env.c_str());
    envptrs.push_back(nullptr);

    Vector<const char*> execparams = { prog_path.data() };
    for (auto& param : params)
        execparams.push_back(param.c_str());
    execparams.push_back(nullptr);

    if (pid_t pid = fork())
        return pid;

    setup_child();

    execve(prog_path.data(), (char* const*)execparams.data(), (char* const*)envptrs.data());
    exit(-1);
    return -1;
}

}

std::pair<String, int> ScopeManager::eval(char const *prog_binary,
    StringView cmdline, const Context& context,
    ShellManager::Flags flags, const ShellContext& shell_context)
{
    using namespace std::chrono;

    const DebugFlags debug_flags = context.options()["debug"].get<DebugFlags>();
    const bool profile = debug_flags & DebugFlags::Profile;
    if (debug_flags & DebugFlags::Shell)
        write_to_debug_buffer(format("scope({}):\n{}\n----\n", prog_binary, cmdline));

    ConstArrayView<String> kak_env = {};

    auto start_time = profile ? steady_clock::now() : steady_clock::time_point{};

    auto spawn_time = profile ? steady_clock::now() : steady_clock::time_point{};

    ShellManager::Pipe child_stdin, child_stdout, child_stderr;
    pid_t pid = spawn_interpreter(format("/bin/{}", prog_binary),
                                  shell_context.params,
                                  kak_env,
                            [&child_stdin, &child_stdout, &child_stderr] {
        auto move = [](int oldfd, int newfd) { dup2(oldfd, newfd); close(oldfd); };

        if (child_stdin.read_fd() != -1)
        {
            close(child_stdin.write_fd());
            move(child_stdin.read_fd(), 0);
        }

        close(child_stdout.read_fd());
        move(child_stdout.write_fd(), 1);

        close(child_stderr.read_fd());
        move(child_stderr.write_fd(), 2);
    });

    child_stdin.close_read_fd();
    child_stdout.close_write_fd();
    child_stderr.close_write_fd();

    write(child_stdin.write_fd(), cmdline);
    child_stdin.close_write_fd();

    auto wait_time = profile ? steady_clock::now() : steady_clock::time_point{};

    String stdout_contents, stderr_contents;
    ShellManager::PipeReader stdout_reader{child_stdout, stdout_contents};
    ShellManager::PipeReader stderr_reader{child_stderr, stderr_contents};

    // block SIGCHLD to make sure we wont receive it before
    // our call to pselect, that will end up blocking indefinitly.
    sigset_t mask, orig_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &orig_mask);
    auto restore_mask = on_scope_end([&] { sigprocmask(SIG_SETMASK, &orig_mask, nullptr); });

    int status = 0;
    // check for termination now that SIGCHLD is blocked
    bool terminated = waitpid(pid, &status, WNOHANG);

    while (not terminated or
           ((flags & ShellManager::Flags::WaitForStdout) and
            (child_stdout.read_fd() != -1 or child_stderr.read_fd() != -1)))
    {
        EventManager::instance().handle_next_events(EventMode::Urgent, &orig_mask);
        if (not terminated)
            terminated = waitpid(pid, &status, WNOHANG);
    }

    if (not stderr_contents.empty())
        write_to_debug_buffer(format("shell stderr: <<<\n{}>>>", stderr_contents));

    if (profile)
    {
        auto end_time = steady_clock::now();
        auto full = duration_cast<milliseconds>(end_time - start_time);
        auto spawn = duration_cast<milliseconds>(wait_time - spawn_time);
        auto wait = duration_cast<milliseconds>(end_time - wait_time);
        write_to_debug_buffer(format("shell execution took {} ms (spawn: {}, wait: {})",
                                     (size_t)full.count(), (size_t)spawn.count(), (size_t)wait.count()));
    }

    return { stdout_contents, WIFEXITED(status) ? WEXITSTATUS(status) : -1 };
}

void ScopeManager::register_env_var(StringView str, bool prefix,
                                    EnvVarRetriever retriever)
{
    m_env_vars.push_back({ str.str(), prefix, std::move(retriever) });
}

String ScopeManager::get_val(StringView name, const Context& context) const
{
    auto env_var = std::find_if(
        m_env_vars.begin(), m_env_vars.end(),
        [name](const EnvVarDesc& desc) {
            return desc.prefix ? prefix_match(name, desc.str) : name == desc.str;
        });

    if (env_var == m_env_vars.end())
        throw runtime_error("no such env var: " + name);

    return env_var->func(name, context);
}

CandidateList ScopeManager::complete_env_var(StringView prefix,
                                             ByteCount cursor_pos) const
{
    return complete(prefix, cursor_pos, m_env_vars |
                    transform([](const EnvVarDesc& desc) -> const String&
                              { return desc.str; }));
}

}
