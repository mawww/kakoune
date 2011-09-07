#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <functional>


namespace Kakoune
{

struct wrong_argument_count : public std::runtime_error
{
    wrong_argument_count()
        : std::runtime_error("wrong argument count") {}
};

typedef std::vector<std::string> CommandParameters;
typedef std::function<void (const CommandParameters&)> Command;

class CommandManager
{
public:
    void execute(const std::string& command_line);

    void register_command(const std::string& command_name, Command command);
    void register_command(const std::vector<std::string>& command_names, Command command);

private:
    std::unordered_map<std::string, Command> m_commands;
};

}

#endif // command_manager_hh_INCLUDED
