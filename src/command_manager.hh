#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include "completion.hh"
#include "memoryview.hh"
#include "shell_manager.hh"
#include "parameters_parser.hh"
#include "string.hh"
#include "utils.hh"

#include <unordered_map>
#include <functional>
#include <initializer_list>

namespace Kakoune
{

class Context;
using CommandParameters = memoryview<String>;
using Command = std::function<void (const ParametersParser& parser, Context& context)>;
using CommandCompleter = std::function<Completions (const Context& context,
                                                    CompletionFlags,
                                                    CommandParameters,
                                                    size_t, ByteCount)>;
enum class CommandFlags
{
    None   = 0,
    Hidden = 1,
};
constexpr CommandFlags operator|(CommandFlags lhs, CommandFlags rhs)
{
    return (CommandFlags)((int)lhs | (int)rhs);
}
constexpr bool operator&(CommandFlags lhs, CommandFlags rhs)
{
    return (bool)((int)lhs & (int)rhs);
}

class PerArgumentCommandCompleter
{
public:
    using ArgumentCompleter = std::function<Completions (const Context&,
                                            CompletionFlags flags,
                                            const String&, ByteCount)>;
    using ArgumentCompleterList = memoryview<ArgumentCompleter>;

    PerArgumentCommandCompleter(ArgumentCompleterList completers)
        : m_completers(completers.begin(), completers.end()) {}

    Completions operator()(const Context& context,
                           CompletionFlags flags,
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

    Completions complete(const Context& context, CompletionFlags flags,
                         const String& command_line, ByteCount cursor_pos);

    bool command_defined(const String& command_name) const;

    void register_command(String command_name, Command command,
                          ParameterDesc param_desc,
                          CommandFlags flags = CommandFlags::None,
                          CommandCompleter completer = CommandCompleter());

    void register_commands(memoryview<String> command_names, Command command,
                           ParameterDesc param_desc,
                           CommandFlags flags = CommandFlags::None,
                           CommandCompleter completer = CommandCompleter());

private:
    void execute_single_command(CommandParameters params,
                                Context& context) const;

    struct CommandDescriptor
    {
        Command command;
        ParameterDesc param_desc;
        CommandFlags flags;
        CommandCompleter completer;
    };
    using CommandMap = std::unordered_map<String, CommandDescriptor>;
    CommandMap m_commands;
    std::unordered_map<String, String> m_aliases;

    CommandMap::const_iterator find_command(const String& name) const;
};

}

#endif // command_manager_hh_INCLUDED
