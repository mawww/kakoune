#include "command_manager.hh"

#include "utils.hh"
#include "assert.hh"

#include <algorithm>

namespace Kakoune
{

void CommandManager::register_command(const std::string& command_name, Command command,
                                      const CommandCompleter& completer)
{
    m_commands[command_name] = CommandAndCompleter { command, completer };
}

void CommandManager::register_command(const std::vector<std::string>& command_names, Command command,
                                      const CommandCompleter& completer)
{
    for (auto command_name : command_names)
        register_command(command_name, command, completer);
}

typedef std::vector<std::pair<size_t, size_t>> TokenList;
static TokenList split(const std::string& line)
{
    TokenList result;

    size_t pos = 0;
    while (pos < line.length())
    {
        while(line[pos] == ' ' and pos != line.length())
            ++pos;

        char delimiter = ' ';
        if (line[pos] == '"' or line[pos] == '\'')
        {
            delimiter = line[pos];
            ++pos;
        }

        size_t token_start = pos;

        while ((line[pos] != delimiter or line[pos-1] == '\\') and
               pos != line.length())
            ++pos;

        result.push_back(std::make_pair(token_start, pos));

        ++pos;
    }
    return result;
}

struct command_not_found : runtime_error
{
    command_not_found(const std::string& command)
        : runtime_error(command + " : no such command") {}
};

void CommandManager::execute(const std::string& command_line,
                             const Context& context)
{
    TokenList tokens = split(command_line);
    if (tokens.empty())
        return;

    std::string command_name =
        command_line.substr(tokens[0].first,
                            tokens[0].second - tokens[0].first);

    auto command_it = m_commands.find(command_name);
    if (command_it == m_commands.end())
        throw command_not_found(command_name);

    CommandParameters params;
    for (auto it = tokens.begin() + 1; it != tokens.end(); ++it)
    {
        params.push_back(command_line.substr(it->first,
                                             it->second - it->first));
    }

    command_it->second.command(params, context);
}

void CommandManager::execute(const std::string& command,
                             const CommandParameters& params,
                             const Context& context)
{
    auto command_it = m_commands.find(command);
    if (command_it == m_commands.end())
        throw command_not_found(command);

    command_it->second.command(params, context);
}

Completions CommandManager::complete(const std::string& command_line, size_t cursor_pos)
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
        std::string prefix = command_line.substr(cmd_start,
                                                 cursor_pos - cmd_start);

        for (auto& command : m_commands)
        {
            if (command.first.substr(0, prefix.length()) == prefix)
                result.candidates.push_back(command.first);
        }

        return result;
    }

    assert(not tokens.empty());
    std::string command_name =
        command_line.substr(tokens[0].first,
                            tokens[0].second - tokens[0].first);

    auto command_it = m_commands.find(command_name);
    if (command_it == m_commands.end() or not command_it->second.completer)
        return Completions();

    CommandParameters params;
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

    const std::string& argument = token_to_complete < params.size() ?
                                  params[token_to_complete] : std::string();
    return m_completers[token_to_complete](argument, pos_in_token);
}

}
