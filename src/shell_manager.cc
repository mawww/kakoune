#include "shell_manager.hh"

#include "clock.hh"
#include "context.hh"
#include "buffer_utils.hh"
#include "event_manager.hh"
#include "file.hh"

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

extern char **environ;

namespace Kakoune
{

ShellManager::ShellManager()
{
    const char* path = getenv("PATH");
    auto new_path = format("{}:{}", path, split_path(get_kak_binary_path()).first);
    setenv("PATH", new_path.c_str(), 1);
}

namespace
{

struct Pipe
{
    Pipe(bool create = true)
        : m_fd{-1, -1}
    {
        if (create and ::pipe(m_fd) < 0)
            throw runtime_error(format("unable to create pipe (fds: {}/{}; errno: {})", m_fd[0], m_fd[1], ::strerror(errno)));
    }
    ~Pipe() { close_read_fd(); close_write_fd(); }

    int read_fd() const { return m_fd[0]; }
    int write_fd() const { return m_fd[1]; }

    void close_read_fd() { close_fd(m_fd[0]); }
    void close_write_fd() { close_fd(m_fd[1]); }

private:
    void close_fd(int& fd) { if (fd != -1) { close(fd); fd = -1; } }
    int m_fd[2];
};

template<typename Func>
pid_t spawn_shell(StringView cmdline, ConstArrayView<String> params,
                  ConstArrayView<String> kak_env, Func setup_child)
{
    Vector<const char*> envptrs;
    for (char** envp = environ; *envp; ++envp)
        envptrs.push_back(*envp);
    for (auto& env : kak_env)
        envptrs.push_back(env.c_str());
    envptrs.push_back(nullptr);

    const char* shell = "/bin/sh";
    auto cmdlinezstr = cmdline.zstr();
    Vector<const char*> execparams = { shell, "-c", cmdlinezstr };
    if (not params.empty())
        execparams.push_back(shell);
    for (auto& param : params)
        execparams.push_back(param.c_str());
    execparams.push_back(nullptr);

    if (pid_t pid = fork())
        return pid;

    setup_child();

    execve(shell, (char* const*)execparams.data(), (char* const*)envptrs.data());
    exit(-1);
    return -1;
}

Vector<String> generate_env(StringView cmdline, const Context& context, const ShellContext& shell_context)
{
    static const Regex re(R"(\bkak_(\w+)\b)");

    Vector<String> kak_env;
    for (RegexIterator<const char*> it{cmdline.begin(), cmdline.end(), re}, end;
         it != end; ++it)
    {
        StringView name{(*it)[1].first, (*it)[1].second};

        auto match_name = [&](const String& s) {
            return s.length() > name.length()  and
                   prefix_match(s, name) and s[name.length()] == '=';
        };
        if (contains_that(kak_env, match_name))
            continue;

        auto var_it = shell_context.env_vars.find(name);
        try
        {
            const String& value = var_it != shell_context.env_vars.end() ?
                var_it->value : ShellManager::instance().get_val(name, context);

            kak_env.push_back(format("kak_{}={}", name, value));
        } catch (runtime_error&) {}
    }

    return kak_env;
}

}

std::pair<String, int> ShellManager::eval(
    StringView cmdline, const Context& context, StringView input,
    Flags flags, const ShellContext& shell_context)
{
    const DebugFlags debug_flags = context.options()["debug"].get<DebugFlags>();
    const bool profile = debug_flags & DebugFlags::Profile;
    if (debug_flags & DebugFlags::Shell)
        write_to_debug_buffer(format("shell:\n{}\n----\n", cmdline));

    auto start_time = profile ? Clock::now() : Clock::time_point{};

    auto kak_env = generate_env(cmdline, context, shell_context);

    auto spawn_time = profile ? Clock::now() : Clock::time_point{};

    Pipe child_stdin{not input.empty()}, child_stdout, child_stderr;
    pid_t pid = spawn_shell(cmdline, shell_context.params, kak_env,
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

    write(child_stdin.write_fd(), input);
    child_stdin.close_write_fd();

    auto wait_time = profile ? Clock::now() : Clock::time_point{};

    struct PipeReader : FDWatcher
    {
        PipeReader(Pipe& pipe, String& contents)
            : FDWatcher(pipe.read_fd(),
                        [&contents, &pipe](FDWatcher& watcher, EventMode) {
                            char buffer[1024];
                            size_t size = ::read(pipe.read_fd(), buffer, 1024);
                            if (size <= 0)
                            {
                                pipe.close_read_fd();
                                watcher.disable();
                                return;
                            }
                            contents += StringView{buffer, buffer+size};
                        })
        {}
    };

    String stdout_contents, stderr_contents;
    PipeReader stdout_reader{child_stdout, stdout_contents};
    PipeReader stderr_reader{child_stderr, stderr_contents};

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
           ((flags & Flags::WaitForStdout) and
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
        using namespace std::chrono;
        auto end_time = Clock::now();
        auto full = duration_cast<milliseconds>(end_time - start_time);
        auto spawn = duration_cast<milliseconds>(wait_time - spawn_time);
        auto wait = duration_cast<milliseconds>(end_time - wait_time);
        write_to_debug_buffer(format("shell execution took {} ms (spawn: {}, wait: {})",
                                     (size_t)full.count(), (size_t)spawn.count(), (size_t)wait.count()));
    }

    return { stdout_contents, WIFEXITED(status) ? WEXITSTATUS(status) : -1 };
}

void ShellManager::register_env_var(StringView str, bool prefix,
                                    EnvVarRetriever retriever)
{
    m_env_vars.push_back({ str.str(), prefix, std::move(retriever) });
}

String ShellManager::get_val(StringView name, const Context& context) const
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

CandidateList ShellManager::complete_env_var(StringView prefix,
                                             ByteCount cursor_pos) const
{
    return complete(prefix, cursor_pos, m_env_vars |
                    transform([](const EnvVarDesc& desc) -> const String&
                              { return desc.str; }));
}

}
