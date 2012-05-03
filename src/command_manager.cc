#include "command_manager.hh"

#include "utils.hh"
#include "assert.hh"
#include "context.hh"
#include "shell_manager.hh"

#include <algorithm>

namespace Kakoune
{

void CommandManager::register_command(const String& command_name, Command command,
                                      unsigned flags,
                                      const CommandCompleter& completer)
{
    m_commands[command_name] = CommandDescriptor { command, flags, completer };
}

void CommandManager::register_commands(const memoryview<String>& command_names, Command command,
                                       unsigned flags,
                                       const CommandCompleter& completer)
{
    for (auto command_name : command_names)
        register_command(command_name, command, flags, completer);
}

static bool is_blank(char c)
{
   return c == ' ' or c == '\t' or c == '\n';
}

typedef std::vector<std::pair<size_t, size_t>> TokenList;
static TokenList split(const String& line)
{
    TokenList result;

    size_t pos = 0;
    while (pos < line.length())
    {
        while(is_blank(line[pos]) and pos != line.length())
            ++pos;

        size_t token_start = pos;

        if (line[pos] == '"' or line[pos] == '\'' or line[pos] == '`')
        {
            char delimiter = line[pos];
            ++pos;
            token_start = delimiter == '`' ? pos - 1 : pos;

            while ((line[pos] != delimiter or line[pos-1] == '\\') and
                    pos != line.length())
                ++pos;

            if (delimiter == '`' and line[pos] == '`')
                ++pos;
        }
        else
            while (not is_blank(line[pos]) and pos != line.length() and
                   (line[pos] != ';' or line[pos-1] == '\\'))
                ++pos;

        if (token_start != pos)
            result.push_back(std::make_pair(token_start, pos));

        if (line[pos] == ';')
           result.push_back(std::make_pair(pos, pos+1));

        ++pos;
    }
    return result;
}

struct command_not_found : runtime_error
{
    command_not_found(const String& command)
        : runtime_error(command + " : no such command") {}
};

void CommandManager::execute(const String& command_line,
                             const Context& context)
{
    TokenList tokens = split(command_line);
    if (tokens.empty())
        return;

    std::vector<String> params;
    for (auto it = tokens.begin(); it != tokens.end(); ++it)
    {
        params.push_back(command_line.substr(it->first,
                                             it->second - it->first));
    }

    execute(params, context);
}

static void shell_eval(std::vector<String>& params,
                       const String& cmdline,
                       const Context& context)
{
        String output = ShellManager::instance().eval(cmdline, context);
        TokenList tokens = split(output);

        for (auto it = tokens.begin(); it != tokens.end(); ++it)
        {
            params.push_back(output.substr(it->first,
                                           it->second - it->first));
        }
}

void CommandManager::execute(const CommandParameters& params,
                             const Context& context)
{
    if (params.empty())
        return;

    auto begin = params.begin();
    auto end = begin;
    while (true)
    {
        while (end != params.end() and *end != ";")
            ++end;

        if (end != begin)
        {
            std::vector<String> expanded_params;
            auto command_it = m_commands.find(*begin);

            if (command_it == m_commands.end() and
                begin->front() == '`' and begin->back() == '`')
            {
                shell_eval(expanded_params,
                           begin->substr(1, begin->length() - 2),
                           context);
                if (not expanded_params.empty())
                {
                    command_it = m_commands.find(expanded_params[0]);
                    expanded_params.erase(expanded_params.begin());
                }
            }

            if (command_it == m_commands.end())
                throw command_not_found(*begin);

            if (command_it->second.flags & IgnoreSemiColons)
                end = params.end();

            if (command_it->second.flags & DeferredShellEval)
                command_it->second.command(CommandParameters(begin + 1, end), context);
            else
            {
                for (auto param = begin+1; param != end; ++param)
                {
                    if (param->front() == '`' and param->back() == '`')
                        shell_eval(expanded_params,
                                   param->substr(1, param->length() - 2),
                                   context);
                    else
                        expanded_params.push_back(*param);
                }
                command_it->second.command(expanded_params, context);
            }

        }

        if (end == params.end())
            break;

        begin = end+1;
        end   = begin;
    }
}

Completions CommandManager::complete(const String& command_line, size_t cursor_pos)
{
    TokenList tokens = split(command_line);

    size_t token_to_complete = tokens.size();
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].first <= cursor_pos and tokens[i].second >= cursor_pos)
        {
            token_to_complete = i;
            break;
        }
    }

    if (token_to_complete == 0 or tokens.empty()) // command name completion
    {
        size_t cmd_start = tokens.empty() ? 0 : tokens[0].first;
        Completions result(cmd_start, cursor_pos);
        String prefix = command_line.substr(cmd_start,
                                            cursor_pos - cmd_start);

        for (auto& command : m_commands)
        {
            if (command.first.substr(0, prefix.length()) == prefix)
                result.candidates.push_back(command.first);
        }

        return result;
    }

    assert(not tokens.empty());
    String command_name =
        command_line.substr(tokens[0].first,
                            tokens[0].second - tokens[0].first);

    auto command_it = m_commands.find(command_name);
    if (command_it == m_commands.end() or not command_it->second.completer)
        return Completions();

    std::vector<String> params;
    for (auto it = tokens.begin() + 1; it != tokens.end(); ++it)
    {
        params.push_back(command_line.substr(it->first,
                                             it->second - it->first));
    }

    size_t start = token_to_complete < tokens.size() ?
                   tokens[token_to_complete].first : cursor_pos;
    Completions result(start , cursor_pos);
    size_t cursor_pos_in_token = cursor_pos - start;

    result.candidates = command_it->second.completer(params,
                                                     token_to_complete - 1,
                                                     cursor_pos_in_token);
    return result;
}

CandidateList PerArgumentCommandCompleter::operator()(const CommandParameters& params,
                                                      size_t token_to_complete,
                                                      size_t pos_in_token) const
{
    if (token_to_complete >= m_completers.size())
        return CandidateList();

    // it is possible to try to complete a new argument
    assert(token_to_complete <= params.size());

    const String& argument = token_to_complete < params.size() ?
                             params[token_to_complete] : String();
    return m_completers[token_to_complete](argument, pos_in_token);
}

}
