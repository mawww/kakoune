#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include "completion.hh"
#include "memoryview.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "utils.hh"

#include <unordered_map>
#include <functional>
#include <initializer_list>

namespace Kakoune
{

struct Context;
using CommandParameters = memoryview<String>;
using Command = std::function<void (CommandParameters, Context& context)>;
using CommandCompleter = std::function<CandidateList (const Context& context,
                                                      CommandParameters,
                                                      size_t, ByteCount)>;

class PerArgumentCommandCompleter
{
public:
    using ArgumentCompleter = std::function<CandidateList (const Context&,
                                            const String&, ByteCount)>;
    using ArgumentCompleterList = memoryview<ArgumentCompleter>;

    PerArgumentCommandCompleter(ArgumentCompleterList completers)
        : m_completers(completers.begin(), completers.end()) {}

    CandidateList operator()(const Context& context,
                             CommandParameters params,
                             size_t token_to_complete,
                             ByteCount pos_in_token) const;

private:
    std::vector<ArgumentCompleter> m_completers;
};

class CommandManager : public Singleton<CommandManager>
{
public:
    void execute(const String& command_line, Context& context,
                 memoryview<String> shell_params = {},
                 const EnvVarMap& env_vars = EnvVarMap{});

    Completions complete(const Context& context,
                         const String& command_line, ByteCount cursor_pos);

    bool command_defined(const String& command_name) const;

    void register_command(String command_name,
                          Command command,
                          CommandCompleter completer = CommandCompleter());

    void register_commands(memoryview<String> command_names,
                           Command command,
                           CommandCompleter completer = CommandCompleter());

private:
    void execute_single_command(CommandParameters params,
                                Context& context) const;
    struct CommandDescriptor
    {
        Command command;
        CommandCompleter completer;
    };
    std::unordered_map<String, CommandDescriptor> m_commands;
};

}

#endif // command_manager_hh_INCLUDED
