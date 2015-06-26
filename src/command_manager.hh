#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include "coord.hh"
#include "completion.hh"
#include "flags.hh"
#include "array_view.hh"
#include "shell_manager.hh"
#include "parameters_parser.hh"
#include "string.hh"
#include "utils.hh"
#include "unordered_map.hh"

#include <functional>
#include <initializer_list>

namespace Kakoune
{

class Context;
using CommandParameters = ConstArrayView<String>;
using Command = std::function<void (const ParametersParser& parser, Context& context)>;
using CommandCompleter = std::function<Completions (const Context& context,
                                                    CompletionFlags,
                                                    CommandParameters,
                                                    size_t, ByteCount)>;
using CommandHelper = std::function<String (const Context& context, CommandParameters)>;

enum class CommandFlags
{
    None   = 0,
    Hidden = 1,
};

template<> struct WithBitOps<CommandFlags> : std::true_type {};

class PerArgumentCommandCompleter
{
public:
    using ArgumentCompleter = std::function<Completions (const Context&,
                                            CompletionFlags flags,
                                            const String&, ByteCount)>;
    using ArgumentCompleterList = ConstArrayView<ArgumentCompleter>;

    PerArgumentCommandCompleter(ArgumentCompleterList completers)
        : m_completers(completers.begin(), completers.end()) {}

    Completions operator()(const Context& context,
                           CompletionFlags flags,
                           CommandParameters params,
                           size_t token_to_complete,
                           ByteCount pos_in_token) const;

private:
    Vector<ArgumentCompleter, MemoryDomain::Commands> m_completers;
};

using CommandInfo = std::pair<String, String>;

class CommandManager : public Singleton<CommandManager>
{
public:
    void execute(StringView command_line, Context& context,
                 ConstArrayView<String> shell_params = {},
                 const EnvVarMap& env_vars = EnvVarMap{});

    Completions complete(const Context& context, CompletionFlags flags,
                         StringView command_line, ByteCount cursor_pos);

    Completions complete(const Context& context, CompletionFlags flags,
                         CommandParameters params,
                         size_t token_to_complete, ByteCount pos_in_token);

    CommandInfo command_info(const Context& context,
                             StringView command_line) const;

    bool command_defined(const String& command_name) const;

    void register_command(String command_name, Command command,
                          String docstring,
                          ParameterDesc param_desc,
                          CommandFlags flags = CommandFlags::None,
                          CommandHelper helper = CommandHelper(),
                          CommandCompleter completer = CommandCompleter());

private:
    void execute_single_command(CommandParameters params,
                                Context& context, CharCoord pos) const;

    struct CommandDescriptor
    {
        Command command;
        String docstring;
        ParameterDesc param_desc;
        CommandFlags flags;
        CommandHelper helper;
        CommandCompleter completer;
    };
    using CommandMap = UnorderedMap<String, CommandDescriptor, MemoryDomain::Commands>;
    CommandMap m_commands;

    CommandMap::const_iterator find_command(const Context& context,
                                            const String& name) const;
};

String expand(StringView str, const Context& context,
              ConstArrayView<String> shell_params,
              const EnvVarMap& env_vars);

}

#endif // command_manager_hh_INCLUDED
