#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include <unordered_map>
#include <functional>
#include <initializer_list>

#include "string.hh"
#include "utils.hh"
#include "completion.hh"
#include "memoryview.hh"
#include "shell_manager.hh"

namespace Kakoune
{

struct Context;

struct wrong_argument_count : runtime_error
{
    wrong_argument_count() : runtime_error("wrong argument count") {}
};

typedef memoryview<String> CommandParameters;
typedef std::function<void (const CommandParameters&,
                            const Context& context)> Command;

typedef std::function<CandidateList (const CommandParameters&,
                                     size_t, size_t)> CommandCompleter;

class PerArgumentCommandCompleter
{
public:
    typedef std::function<CandidateList (const String&, size_t)> ArgumentCompleter;
    typedef memoryview<ArgumentCompleter> ArgumentCompleterList;

    PerArgumentCommandCompleter(const ArgumentCompleterList& completers)
        : m_completers(completers.begin(), completers.end()) {}

    CandidateList operator()(const CommandParameters& params,
                             size_t token_to_complete,
                             size_t pos_in_token) const;

private:
    std::vector<ArgumentCompleter> m_completers;
};

class CommandManager : public Singleton<CommandManager>
{
public:
    enum Flags
    {
        None = 0,
        IgnoreSemiColons = 1,
        DeferredShellEval = 2,
    };

    void execute(const String& command_line, const Context& context,
                 const EnvVarMap& env_vars = EnvVarMap());
    void execute(const CommandParameters& params, const Context& context,
                 const EnvVarMap& env_vars = EnvVarMap());

    Completions complete(const String& command_line, size_t cursor_pos);

    void register_command(const String& command_name,
                          Command command,
                          unsigned flags = None,
                          const CommandCompleter& completer = CommandCompleter());

    void register_commands(const memoryview<String>& command_names,
                           Command command,
                           unsigned flags = None,
                           const CommandCompleter& completer = CommandCompleter());

private:
    struct CommandDescriptor
    {
        Command command;
        unsigned flags;
        CommandCompleter completer;
    };
    std::unordered_map<String, CommandDescriptor> m_commands;
};

}

#endif // command_manager_hh_INCLUDED
