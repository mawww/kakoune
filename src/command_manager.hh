#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "exception.hh"


namespace Kakoune
{

struct wrong_argument_count : runtime_error
{
    wrong_argument_count() : runtime_error("wrong argument count") {}
};

typedef std::vector<std::string> CommandParameters;
typedef std::function<void (const CommandParameters&)> Command;

struct Completions
{
    CommandParameters candidates;
    size_t start;
    size_t end;

    Completions(size_t start, size_t end)
        : start(start), end(end) {}
};

class CommandManager
{
public:
    void execute(const std::string& command_line);
    Completions complete(const std::string& command_line, size_t cursor_pos);

    void register_command(const std::string& command_name, Command command);
    void register_command(const std::vector<std::string>& command_names, Command command);

private:
    std::unordered_map<std::string, Command> m_commands;
};

}

#endif // command_manager_hh_INCLUDED
