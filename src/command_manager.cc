#include "command_manager.hh"

#include "utils.hh"
#include <algorithm>

namespace Kakoune
{

void CommandManager::register_command(const std::string& command_name, Command command)
{
    m_commands[command_name] = command;
}

void CommandManager::register_command(const std::vector<std::string>& command_names, Command command)
{
    for (auto command_name : command_names)
        register_command(command_name, command);
}

typedef std::vector<std::pair<size_t, size_t>> TokenList;
static TokenList split(const std::string& line)
{
    TokenList result;

    size_t pos = 0;
    while (pos != line.length())
    {
        while(line[pos] == ' ' and pos != line.length())
            ++pos;

        size_t token_start = pos;

        while((line[pos] != ' ' or line[pos-1] == '\\') and pos != line.length())
            ++pos;

        result.push_back(std::make_pair(token_start, pos));
    }
    return result;
}

struct command_not_found : runtime_error
{
    command_not_found(const std::string& command)
        : runtime_error(command + " : no such command") {}
};

void CommandManager::execute(const std::string& command_line)
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

    command_it->second(params);
}

Completions CommandManager::complete(const std::string& command_line, size_t cursor_pos)
{
    TokenList tokens = split(command_line);

    size_t token_to_complete = -1;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].first < cursor_pos and tokens[i].second >= cursor_pos)
        {
            token_to_complete = i;
            break;
        }
    }

    if (token_to_complete == 0) // command name completion
    {
        Completions result(tokens[0].first, cursor_pos);
        std::string prefix = command_line.substr(tokens[0].first,
                                                 cursor_pos - tokens[0].first);

        for (auto& command : m_commands)
        {
            if (command.first.substr(0, prefix.length()) == prefix)
                result.candidates.push_back(command.first);
        }

        return result;
    }
    if (token_to_complete == 1) // filename completion
    {
        Completions result(tokens[1].first, cursor_pos);
        std::string prefix = command_line.substr(tokens[1].first, cursor_pos);
        result.candidates = complete_filename(prefix);
        return result;
    }
    return Completions(cursor_pos, cursor_pos);
}

}
