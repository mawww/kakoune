#include "shell_manager.hh"

#include "context.hh"
#include "debug.hh"
#include "event_manager.hh"
#include "file.hh"

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Kakoune
{

static const Regex env_var_regex(R"(\bkak_(\w+)\b)");

ShellManager::ShellManager()
{
    const char* path = getenv("PATH");
    String new_path;
    if (path)
        new_path = path + ":"_str;

    new_path += split_path(get_kak_binary_path()).first;
    setenv("PATH", new_path.c_str(), 1);
}

std::pair<String, int> ShellManager::eval(
    StringView cmdline, const Context& context, StringView input,
    ConstArrayView<String> params, const EnvVarMap& env_vars)
{
    int write_pipe[2]; // child stdin
    int read_pipe[2];  // child stdout
    int error_pipe[2]; // child stderr

    ::pipe(write_pipe);
    ::pipe(read_pipe);
    ::pipe(error_pipe);

    if (pid_t pid = fork())
    {
        close(write_pipe[0]);
        close(read_pipe[1]);
        close(error_pipe[1]);

        write(write_pipe[1], input.data(), (int)input.length());
        close(write_pipe[1]);

        String child_stdout, child_stderr;
        {
            auto pipe_reader = [](String& output) {
                return [&output](FDWatcher& watcher, EventMode) {
                    char buffer[1024];
                    size_t size = read(watcher.fd(), buffer, 1024);
                    if (size <= 0)
                        watcher.close_fd();
                    output += StringView{buffer, buffer+size};
                };
            };

            FDWatcher stdout_watcher{read_pipe[0], pipe_reader(child_stdout)};
            FDWatcher stderr_watcher{error_pipe[0], pipe_reader(child_stderr)};

            while (not stdout_watcher.closed() or not stderr_watcher.closed())
                EventManager::instance().handle_next_events(EventMode::Urgent);
        }

        if (not child_stderr.empty())
            write_debug("shell stderr: <<<\n" + child_stderr + ">>>");

        int status = 0;
        waitpid(pid, &status, 0);
        return { child_stdout, WIFEXITED(status) ? WEXITSTATUS(status) : - 1 };
    }
    else try
    {
        close(write_pipe[1]);
        close(read_pipe[0]);
        close(error_pipe[0]);

        dup2(read_pipe[1], 1); close(read_pipe[1]);
        dup2(error_pipe[1], 2); close(error_pipe[1]);
        dup2(write_pipe[0], 0); close(write_pipe[0]);

        using RegexIt = RegexIterator<StringView::iterator>;
        for (RegexIt it{cmdline.begin(), cmdline.end(), env_var_regex}, end;
             it != end; ++it)
        {
            auto& match = *it;

            StringView name{match[1].first, match[1].second};
            kak_assert(name.length() > 0);

            auto local_var = env_vars.find(name.str());
            if (local_var != env_vars.end())
                setenv(("kak_" + name).c_str(), local_var->second.c_str(), 1);
            else try
            {
                String value = get_val(name, context);
                setenv(("kak_"_str + name).c_str(), value.c_str(), 1);
            }
            catch (runtime_error&) {}
        }
        const char* shell = "/bin/sh";
        auto cmdlinezstr = cmdline.zstr();
        Vector<const char*> execparams = { shell, "-c", cmdlinezstr };
        if (not params.empty())
            execparams.push_back(shell);
        for (auto& param : params)
            execparams.push_back(param.c_str());
        execparams.push_back(nullptr);

        execvp(shell, (char* const*)execparams.data());
        exit(-1);
    }
    catch (...) { exit(-1); }
    return {};
}

void ShellManager::register_env_var(StringView regex,
                                    EnvVarRetriever retriever)
{
    m_env_vars.push_back({ Regex(regex.begin(), regex.end()), std::move(retriever) });
}

String ShellManager::get_val(StringView name, const Context& context) const
{
    auto env_var = std::find_if(
        m_env_vars.begin(), m_env_vars.end(),
        [&](const std::pair<Regex, EnvVarRetriever>& pair)
        { return regex_match(name.begin(), name.end(), pair.first); });

    if (env_var == m_env_vars.end())
        throw runtime_error("no such env var: " + name);
    return env_var->second(name, context);
}

}
