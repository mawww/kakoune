#include "shell_manager.hh"

#include "debug.hh"

#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>

namespace Kakoune
{
String env_var_regex(R"(\$\{kak_(\w+)[^}]*\}|\$kak_(\w+))");

ShellManager::ShellManager()
   : m_regex(env_var_regex.begin(), env_var_regex.end())
{
}

String ShellManager::eval(const String& cmdline, const Context& context,
                          const memoryview<String>& params,
                          const EnvVarMap& env_vars)
{
    return pipe("", cmdline, context, params, env_vars);
}

String ShellManager::pipe(const String& input,
                          const String& cmdline, const Context& context,
                          const memoryview<String>& params,
                          const EnvVarMap& env_vars)
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

        memoryview<char> data = input.data();
        write(write_pipe[1], data.pointer(), data.size());
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
        {
            write_debug("\nshell stderr: <<<\n" + errorout + ">>>\n");
        }

        waitpid(pid, NULL, 0);
    }
    else try
    {
        close(write_pipe[1]);
        close(read_pipe[0]);
        close(error_pipe[0]);

        dup2(read_pipe[1], 1); close(read_pipe[1]);
        dup2(error_pipe[1], 2); close(error_pipe[1]);
        dup2(write_pipe[0], 0); close(write_pipe[0]);

        boost::regex_iterator<String::iterator> it(cmdline.begin(), cmdline.end(), m_regex);
        boost::regex_iterator<String::iterator> end;

        while (it != end)
        {
            auto& match = *it;

            String name;
            if (match[1].matched)
                name = String(match[1].first, match[1].second);
            else if (match[2].matched)
                name = String(match[2].first, match[2].second);
            else
                assert(false);
            assert(name.length() > 0);

            auto local_var = env_vars.find(name);
            if (local_var != env_vars.end())
                setenv(("kak_" + name).c_str(), local_var->second.c_str(), 1);
            else
            {
                auto env_var = std::find_if(
                    m_env_vars.begin(), m_env_vars.end(),
                    [&](const std::pair<Regex, EnvVarRetriever>& pair)
                    { return boost::regex_match(name.begin(), name.end(),
                                                pair.first); });

                if (env_var != m_env_vars.end())
                {
                    String value = env_var->second(name, context);
                    setenv(("kak_" + name).c_str(), value.c_str(), 1);
                }
            }

            ++it;
        }
        std::vector<const char*> execparams = { "sh", "-c", cmdline.c_str() };
        if (not params.empty())
            execparams.push_back("sh");
        for (auto& param : params)
            execparams.push_back(param.c_str());
        execparams.push_back(NULL);

        execvp("sh", (char* const*)execparams.data());
        exit(-1);
    }
    catch (...) { exit(-1); }
    return output;
}

void ShellManager::register_env_var(const String& regex,
                                    EnvVarRetriever retriever)
{
    m_env_vars.push_back({ Regex(regex.begin(), regex.end()), std::move(retriever) });
}

}
