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

static std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> result;

    size_t pos = 0;
    while (pos != line.length())
    {
        while(line[pos] == ' ' and pos != line.length())
            ++pos;

        size_t token_start = pos;

        while((line[pos] != ' ' or line[pos-1] == '\\') and pos != line.length())
            ++pos;

        result.push_back(line.substr(token_start, pos - token_start));
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
    std::vector<std::string> tokens = split(command_line);
    if (tokens.empty())
        return;

    auto command_it = m_commands.find(tokens[0]);
    if (command_it == m_commands.end())
        throw command_not_found(tokens[0]);

    CommandParameters params(tokens.begin() + 1, tokens.end());
    command_it->second(params);
}

}
