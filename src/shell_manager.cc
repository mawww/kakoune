#include "shell_manager.hh"

#include "buffer_utils.hh"
#include "client.hh"
#include "clock.hh"
#include "context.hh"
#include "command_manager.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "flags.hh"
#include "option.hh"
#include "option_types.hh"
#include "regex.hh"

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <errno.h>

#if defined(__CYGWIN__)
#define vfork fork
#endif

extern char **environ;

namespace Kakoune
{

ShellManager::ShellManager(ConstArrayView<EnvVarDesc> builtin_env_vars)
    : m_env_vars{builtin_env_vars}
{
    auto is_executable = [](StringView path) {
        struct stat st;
        if (stat(path.zstr(), &st))
            return false;

        bool executable = (st.st_mode & S_IXUSR)
                        | (st.st_mode & S_IXGRP)
                        | (st.st_mode & S_IXOTH);
        return S_ISREG(st.st_mode) and executable;
    };

    if (const char* shell = getenv("KAKOUNE_POSIX_SHELL"))
    {
        if (not is_executable(shell))
            throw runtime_error{format("KAKOUNE_POSIX_SHELL '{}' is not executable", shell)};
        m_shell = shell;
    }
    else // Get a guaranteed to be POSIX shell binary
    {
        #if defined(_CS_PATH)
        auto size = confstr(_CS_PATH, nullptr, 0);
        String path; path.resize(size-1, 0);
        confstr(_CS_PATH, path.data(), size);
        #else
        StringView path = "/bin:/usr/bin";
        #endif
        for (auto dir : StringView{path} | split<StringView>(':'))
        {
            auto candidate = format("{}/sh", dir);
            if (is_executable(candidate))
            {
                m_shell = std::move(candidate);
                break;
            }
        }
        if (m_shell.empty())
            throw runtime_error{format("unable to find a posix shell in {}", path)};
    }

    // Add Kakoune binary location to the path to guarantee that %sh{ ... }
    // have access to the kak command regardless of if the user installed it
    {
        const char* path = getenv("PATH");
        auto new_path = format("{}../libexec/kak:{}", split_path(get_kak_binary_path()).first, path);
        setenv("PATH", new_path.c_str(), 1);
    }
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
pid_t spawn_shell(const char* shell, StringView cmdline,
                  ConstArrayView<String> params,
                  ConstArrayView<String> kak_env,
                  Func setup_child) noexcept
{
    Vector<const char*> envptrs;
    for (char** envp = environ; *envp; ++envp)
        envptrs.push_back(*envp);
    for (auto& env : kak_env)
        envptrs.push_back(env.c_str());
    envptrs.push_back(nullptr);

    auto cmdlinezstr = cmdline.zstr();
    Vector<const char*> execparams = { "sh", "-c", cmdlinezstr };
    if (not params.empty())
        execparams.push_back(shell);
    for (auto& param : params)
        execparams.push_back(param.c_str());
    execparams.push_back(nullptr);

    if (pid_t pid = vfork())
        return pid;

    setup_child();

    execve(shell, (char* const*)execparams.data(), (char* const*)envptrs.data());
    char buffer[1024];
    write(STDERR_FILENO, format_to(buffer, "execve failed: {}\n", strerror(errno)));
    _exit(-1);
    return -1;
}

template<typename GetValue>
Vector<String> generate_env(StringView cmdline, const Context& context, GetValue&& get_value)
{
    static const Regex re(R"(\bkak_(quoted_)?(\w+)\b)");

    Vector<String> env;
    for (auto&& match : RegexIterator{cmdline.begin(), cmdline.end(), re})
    {
        StringView name{match[2].first, match[2].second};
        StringView shell_name{match[0].first, match[0].second};

        auto match_name = [&](const String& s) {
            return s.substr(0_byte, shell_name.length()) == shell_name and
                   s.substr(shell_name.length(), 1_byte) == "=";
        };
        if (any_of(env, match_name))
            continue;

        try
        {
            StringView quoted{match[1].first, match[1].second};
            Quoting quoting = match[1].matched ? Quoting::Shell : Quoting::Raw;
            env.push_back(format("kak_{}{}={}", quoted, name, get_value(name, quoting)));
        } catch (runtime_error&) {}
    }

    return env;
}

template<typename OnClose>
FDWatcher make_reader(int fd, String& contents, OnClose&& on_close)
{
    return {fd, FdEvents::Read, EventMode::Urgent,
            [&contents, on_close](FDWatcher& watcher, FdEvents, EventMode) {
        const int fd = watcher.fd();
        char buffer[1024];
        while (fd_readable(fd))
        {
            ssize_t size = ::read(fd, buffer, sizeof(buffer));
            if (size <= 0)
            {
                if (size < 0 and errno == EAGAIN)
                    continue; // try again

                watcher.disable();
                on_close(size == 0);
                return;
            }
            contents += StringView{buffer, buffer+size};
        }
    }};
}

FDWatcher make_pipe_writer(Pipe& pipe, StringView contents)
{
    int flags = fcntl(pipe.write_fd(), F_GETFL, 0);
    fcntl(pipe.write_fd(), F_SETFL, flags | O_NONBLOCK);
    return {pipe.write_fd(), FdEvents::Write, EventMode::Urgent,
            [contents, &pipe](FDWatcher& watcher, FdEvents, EventMode) mutable {
        while (fd_writable(pipe.write_fd()))
        {
            ssize_t size = ::write(pipe.write_fd(), contents.begin(),
                                   (size_t)contents.length());
            if (size > 0)
                contents = contents.substr(ByteCount{(int)size});
            if (size == -1 and (errno == EAGAIN or errno == EWOULDBLOCK))
                return;
            if (size < 0 or contents.empty())
            {
                pipe.close_write_fd();
                watcher.disable();
                return;
            }
        }
    }};
}

struct CommandFifos
{
    String base_dir;
    String command;
    FDWatcher command_watcher;

    CommandFifos(Context& context, const ShellContext& shell_context)
      : base_dir(format("{}/kak-fifo.XXXXXX", tmpdir())),
        command_watcher([&] {
            mkdtemp(base_dir.data()),
            mkfifo(command_fifo_path().c_str(), 0600);
            mkfifo(response_fifo_path().c_str(), 0600);
            int fd = open(command_fifo_path().c_str(), O_RDONLY | O_NONBLOCK);
            return make_reader(fd, command, [&, fd](bool graceful) {
                if (not graceful)
                {
                    write_to_debug_buffer(format("error reading from command fifo '{}'", strerror(errno)));
                    return;
                }
                CommandManager::instance().execute(command, context, shell_context);
                command.clear();
                command_watcher.reset_fd(fd);
            });
        }())
    {
    }

    ~CommandFifos()
    {
        command_watcher.close_fd();
        unlink(command_fifo_path().c_str());
        unlink(response_fifo_path().c_str());
        rmdir(base_dir.c_str());
    }

    String command_fifo_path() const { return format("{}/command-fifo", base_dir); }
    String response_fifo_path() const { return format("{}/response-fifo", base_dir); }
};

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

    Optional<CommandFifos> command_fifos;

    auto kak_env = generate_env(cmdline, context, [&](StringView name, Quoting quoting) {
        if (name == "command_fifo" or name == "response_fifo")
        {
            if (not command_fifos)
                command_fifos.emplace(const_cast<Context&>(context), shell_context);
            return name == "command_fifo" ?
                command_fifos->command_fifo_path() : command_fifos->response_fifo_path();
        }

        if (auto it = shell_context.env_vars.find(name); it != shell_context.env_vars.end())
            return it->value;
        return join(get_val(name, context) | transform(quoter(quoting)), ' ', false);
    });

    auto spawn_time = profile ? Clock::now() : Clock::time_point{};

    Pipe child_stdin{not input.empty()}, child_stdout, child_stderr;
    pid_t pid = spawn_shell(m_shell.c_str(), cmdline, shell_context.params, kak_env,
                            [&child_stdin, &child_stdout, &child_stderr] {
        auto move = [](int oldfd, int newfd)
        {
            if (oldfd == newfd)
                return;
            dup2(oldfd, newfd); close(oldfd);
        };

        if (child_stdin.read_fd() != -1)
        {
            close(child_stdin.write_fd());
            move(child_stdin.read_fd(), 0);
        }
        else
            move(open("/dev/null", O_RDONLY), 0);

        close(child_stdout.read_fd());
        move(child_stdout.write_fd(), 1);

        close(child_stderr.read_fd());
        move(child_stderr.write_fd(), 2);
    });

    child_stdin.close_read_fd();
    child_stdout.close_write_fd();
    child_stderr.close_write_fd();

    auto wait_time = Clock::now();

    String stdout_contents, stderr_contents;
    auto stdout_reader = make_reader(child_stdout.read_fd(), stdout_contents, [&](bool){ child_stdout.close_read_fd(); });
    auto stderr_reader = make_reader(child_stderr.read_fd(), stderr_contents, [&](bool){ child_stderr.close_read_fd(); });
    auto stdin_writer = make_pipe_writer(child_stdin, input);

    // block SIGCHLD to make sure we wont receive it before
    // our call to pselect, that will end up blocking indefinitly.
    sigset_t mask, orig_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &orig_mask);
    auto restore_mask = on_scope_end([&] { sigprocmask(SIG_SETMASK, &orig_mask, nullptr); });

    int status = 0;
    // check for termination now that SIGCHLD is blocked
    bool terminated = waitpid(pid, &status, WNOHANG) != 0;
    bool failed = false;

    using namespace std::chrono;
    static constexpr seconds wait_timeout{1};
    Optional<DisplayLine> previous_status;
    Timer wait_timer{wait_time + wait_timeout, [&](Timer& timer) {
        if (not context.has_client())
            return;

        const auto now = Clock::now();
        timer.set_next_date(now + wait_timeout);
        auto& client = context.client();
        if (not previous_status)
            previous_status = client.current_status();

        client.print_status({format("waiting for shell command to finish{} ({}s)",
                                     terminated ? " (shell terminated)" : "",
                                     duration_cast<seconds>(now - wait_time).count()),
                             context.faces()[failed ? "Error" : "Information"]});
        client.redraw_ifn();
    }, EventMode::Urgent};

    while (not terminated or child_stdin.write_fd() != -1 or
           ((flags & Flags::WaitForStdout) and
            (child_stdout.read_fd() != -1 or child_stderr.read_fd() != -1)))
    {
        try
        {
            EventManager::instance().handle_next_events(EventMode::Urgent, &orig_mask);
        }
        catch (runtime_error& error)
        {
            write_to_debug_buffer(format("error while waiting for shell: {}", error.what()));
            failed = true;
        }
        if (not terminated)
            terminated = waitpid(pid, &status, WNOHANG) == pid;
    }

    if (not stderr_contents.empty())
        write_to_debug_buffer(format("shell stderr: <<<\n{}>>>", stderr_contents));

    if (profile)
    {
        auto end_time = Clock::now();
        auto full = duration_cast<microseconds>(end_time - start_time);
        auto spawn = duration_cast<microseconds>(wait_time - spawn_time);
        auto wait = duration_cast<microseconds>(end_time - wait_time);
        write_to_debug_buffer(format("shell execution took {} us (spawn: {}, wait: {})",
                                     (size_t)full.count(), (size_t)spawn.count(), (size_t)wait.count()));
    }

    if (previous_status) // restore the status line
    {
        context.print_status(std::move(*previous_status));
        context.client().redraw_ifn();
    }

    return { std::move(stdout_contents), WIFEXITED(status) ? WEXITSTATUS(status) : -1 };
}

Vector<String> ShellManager::get_val(StringView name, const Context& context) const
{
    auto env_var = find_if(m_env_vars, [name](const EnvVarDesc& desc) {
        return desc.prefix ? prefix_match(name, desc.str) : name == desc.str;
    });

    if (env_var == m_env_vars.end())
        throw runtime_error("no such variable: " + name);

    return env_var->func(name, context);
}

CandidateList ShellManager::complete_env_var(StringView prefix,
                                             ByteCount cursor_pos) const
{
    return complete(prefix, cursor_pos,
                    m_env_vars | transform(&EnvVarDesc::str));
}

}
