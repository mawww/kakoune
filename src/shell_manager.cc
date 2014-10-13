#include "shell_manager.hh"

#include "context.hh"
#include "debug.hh"

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Kakoune
{

static const Regex env_var_regex(R"(\bkak_(\w+)\b)");

ShellManager::ShellManager()
{
}

String ShellManager::eval(StringView cmdline, const Context& context,
                          memoryview<String> params,
                          const EnvVarMap& env_vars,
                          int* exit_status)
{
    return pipe("", cmdline, context, params, env_vars, exit_status);
}

String ShellManager::pipe(StringView input,
                          StringView cmdline, const Context& context,
                          memoryview<String> params,
                          const EnvVarMap& env_vars,
                          int* exit_status)
{
    int write_pipe[2]; // child stdin
    int read_pipe[2];  // child stdout
    int error_pipe[2]; // child stderr

    ::pipe(write_pipe);
    ::pipe(read_pipe);
    ::pipe(error_pipe);

    String output;
    if (pid_t pid = fork())
    {
        close(write_pipe[0]);
        close(read_pipe[1]);
        close(error_pipe[1]);

        write(write_pipe[1], input.data(), (int)input.length());
        close(write_pipe[1]);

        char buffer[1024];
        while (size_t size = read(read_pipe[0], buffer, 1024))
        {
            if (size == -1)
                break;
            output += String(buffer, buffer+size);
        }
        close(read_pipe[0]);

        String errorout;
        while (size_t size = read(error_pipe[0], buffer, 1024))
        {
            if (size == -1)
                break;
            errorout += String(buffer, buffer+size);
        }
        close(error_pipe[0]);
        if (not errorout.empty())
            write_debug("shell stderr: <<<\n" + errorout + ">>>");

        waitpid(pid, exit_status, 0);
        if (exit_status)
        {
            if (WIFEXITED(*exit_status))
                *exit_status = WEXITSTATUS(*exit_status);
            else
                *exit_status = -1;
        }
    }
    else try
    {
        close(write_pipe[1]);
        close(read_pipe[0]);
        close(error_pipe[0]);

        dup2(read_pipe[1], 1); close(read_pipe[1]);
        dup2(error_pipe[1], 2); close(error_pipe[1]);
        dup2(write_pipe[0], 0); close(write_pipe[0]);

        RegexIterator<StringView::iterator> it(cmdline.begin(), cmdline.end(), env_var_regex);
        RegexIterator<StringView::iterator> end;

        while (it != end)
        {
            auto& match = *it;

            StringView name;
            if (match[1].matched)
                name = StringView(match[1].first, match[1].second);
            else if (match[2].matched)
                name = StringView(match[2].first, match[2].second);
            else
                kak_assert(false);
            kak_assert(name.length() > 0);

            auto local_var = env_vars.find(name);
            if (local_var != env_vars.end())
                setenv(("kak_" + name).c_str(), local_var->second.c_str(), 1);
            else
            {
                try
                {
                    String value = get_val(name, context);
                    setenv(("kak_"_str + name).c_str(), value.c_str(), 1);
                }
                catch (runtime_error&) {}
            }

            ++it;
        }
        const char* shell = "/bin/sh";
        auto cmdlinezstr = cmdline.zstr();
        std::vector<const char*> execparams = { shell, "-c", cmdlinezstr };
        if (not params.empty())
            execparams.push_back(shell);
        for (auto& param : params)
            execparams.push_back(param.c_str());
        execparams.push_back(nullptr);

        execvp(shell, (char* const*)execparams.data());
        exit(-1);
    }
    catch (...) { exit(-1); }
    return output;
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
