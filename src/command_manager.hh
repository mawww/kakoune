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

struct Token
{
    enum class Type
    {
        Raw,
        ShellExpand,
        CommandSeparator
    };
    Token() : m_type(Type::Raw) {}

    explicit Token(const String& string) : m_content(string), m_type(Type::Raw) {}
    explicit Token(Type type) : m_type(type) {}
    Token(Type type, String str) : m_content(str), m_type(type) {}

    Type type() const { return m_type; }

    const String& content() const { return m_content; }

private:
    Type   m_type;
    String m_content;
};

using CommandParameters = memoryview<Token>;

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
    void execute(const String& command_line, const Context& context,
                 const EnvVarMap& env_vars = EnvVarMap());
    void execute(const CommandParameters& params, const Context& context,
                 const EnvVarMap& env_vars = EnvVarMap());

    Completions complete(const String& command_line, size_t cursor_pos);

    bool command_defined(const String& command_name) const;

    void register_command(const String& command_name,
                          Command command,
                          const CommandCompleter& completer = CommandCompleter());

    void register_commands(const memoryview<String>& command_names,
                           Command command,
                           const CommandCompleter& completer = CommandCompleter());

private:
    struct CommandDescriptor
    {
        Command command;
        CommandCompleter completer;
    };
    std::unordered_map<String, CommandDescriptor> m_commands;
};

}

#endif // command_manager_hh_INCLUDED
