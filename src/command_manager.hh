#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include <string>
#include <unordered_map>
#include <functional>
#include <initializer_list>

#include "utils.hh"
#include "completion.hh"
#include "memoryview.hh"

namespace Kakoune
{

struct Context;

struct wrong_argument_count : runtime_error
{
    wrong_argument_count() : runtime_error("wrong argument count") {}
};

typedef memoryview<std::string> CommandParameters;
typedef std::function<void (const CommandParameters&,
                            const Context& context)> Command;

typedef std::function<CandidateList (const CommandParameters&,
                                     size_t, size_t)> CommandCompleter;

class PerArgumentCommandCompleter
{
public:
    typedef std::function<CandidateList (const std::string&, size_t)> ArgumentCompleter;
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
    };

    void execute(const std::string& command_line, const Context& context);
    void execute(const CommandParameters& params, const Context& context);

    Completions complete(const std::string& command_line, size_t cursor_pos);

    void register_command(const std::string& command_name,
                          Command command,
                          unsigned flags = None,
                          const CommandCompleter& completer = CommandCompleter());

    void register_commands(const memoryview<std::string>& command_names,
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
    std::unordered_map<std::string, CommandDescriptor> m_commands;
};

}

#endif // command_manager_hh_INCLUDED
