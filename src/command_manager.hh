#ifndef command_manager_hh_INCLUDED
#define command_manager_hh_INCLUDED

#include "coord.hh"
#include "completion.hh"
#include "array_view.hh"
#include "shell_manager.hh"
#include "parameters_parser.hh"
#include "string.hh"
#include "optional.hh"
#include "utils.hh"
#include "hash_map.hh"

#include <functional>
#include <initializer_list>

namespace Kakoune
{

class Context;
using CommandParameters = ConstArrayView<String>;
using CommandFunc = std::function<void (const ParametersParser& parser,
                                        Context& context,
                                        const ShellContext& shell_context)>;

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
constexpr bool with_bit_ops(Meta::Type<CommandFlags>) { return true; }

struct CommandInfo { String name, info; };

struct Token
{
    enum class Type
    {
        Raw,
        RawQuoted,
        RawEval,
        ShellExpand,
        RegisterExpand,
        OptionExpand,
        ValExpand,
        ArgExpand,
        FileExpand,
        CommandSeparator
    };

    Type type;
    ByteCount pos;
    BufferCoord coord;
    String content;
    bool terminated = false;
};

struct Reader
{
public:
    Reader(StringView s) : str{s}, pos{s.begin()}, line_start{s.begin()}, line{} {}

    Codepoint operator*() const;
    Codepoint peek_next() const;
    Reader& operator++();
    Reader&  next_byte();

    explicit operator bool() const { return pos < str.end(); }
    StringView substr_from(const char* start) const { return {start, pos}; }
    BufferCoord coord() const { return {line, (int)(pos - line_start)}; }

    StringView str;
    const char* pos;
    const char* line_start;
    LineCount line;
};

class CommandParser
{
public:
    CommandParser(StringView command_line);
    Optional<Token> read_token(bool throw_on_unterminated);

    const char* pos() const { return m_reader.pos; }
    BufferCoord coord() const { return m_reader.coord(); }
    bool done() const { return not m_reader; }

private:
    Reader m_reader;
};

class CommandManager : public Singleton<CommandManager>
{
public:
    void execute(StringView command_line, Context& context,
                 const ShellContext& shell_context = ShellContext{});

    void execute_single_command(CommandParameters params,
                                Context& context,
                                const ShellContext& shell_context,
                                BufferCoord pos = {});


    Completions complete(const Context& context, CompletionFlags flags,
                         StringView command_line, ByteCount cursor_pos);

    Completions complete(const Context& context, CompletionFlags flags,
                         CommandParameters params,
                         size_t token_to_complete, ByteCount pos_in_token);

    Optional<CommandInfo> command_info(const Context& context,
                                       StringView command_line) const;

    bool command_defined(StringView command_name) const;

    void register_command(String command_name, CommandFunc func,
                          String docstring,
                          ParameterDesc param_desc,
                          CommandFlags flags = CommandFlags::None,
                          CommandHelper helper = CommandHelper(),
                          CommandCompleter completer = CommandCompleter());

    Completions complete_command_name(const Context& context, StringView query) const;

    void clear_last_complete_command() { m_last_complete_command = String{}; }

    bool module_defined(StringView module_name) const;

    void register_module(String module_name, String commands);

    void load_module(StringView module_name, Context& context);

    Completions complete_module_name(StringView query) const;

private:
    struct Command
    {
        CommandFunc func;
        String docstring;
        ParameterDesc param_desc;
        CommandFlags flags;
        CommandHelper helper;
        CommandCompleter completer;
    };
    using CommandMap = HashMap<String, Command, MemoryDomain::Commands>;
    CommandMap m_commands;
    String m_last_complete_command;
    int m_command_depth = 0;

    struct Module
    {
        enum class State
        {
            Registered,
            Loading,
            Loaded
        };
        State state = State::Registered;
        String commands;
    };
    using ModuleMap = HashMap<String, Module, MemoryDomain::Commands>;
    ModuleMap m_modules;
};

String expand(StringView str, const Context& context,
              const ShellContext& shell_context = ShellContext{});

String expand(StringView str, const Context& context,
              const ShellContext& shell_context,
              const FunctionRef<String (String)>& postprocess);

}

#endif // command_manager_hh_INCLUDED
