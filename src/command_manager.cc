#include "command_manager.hh"

#include "utils.hh"
#include "assert.hh"
#include "context.hh"
#include "shell_manager.hh"

#include <algorithm>

namespace Kakoune
{

bool CommandManager::command_defined(const String& command_name) const
{
    return m_commands.find(command_name) != m_commands.end();
}

void CommandManager::register_command(const String& command_name,
                                      Command command,
                                      unsigned flags,
                                      const CommandCompleter& completer)
{
    m_commands[command_name] = CommandDescriptor { command, flags, completer };
}

void CommandManager::register_commands(const memoryview<String>& command_names,
                                       Command command,
                                       unsigned flags,
                                       const CommandCompleter& completer)
{
    for (auto command_name : command_names)
        register_command(command_name, command, flags, completer);
}

static bool is_blank(char c)
{
   return c == ' ' or c == '\t' or c == '\n';
}

using TokenList = std::vector<Token>;
using TokenPosList = std::vector<std::pair<size_t, size_t>>;

static TokenList parse(const String& line,
                       TokenPosList* opt_token_pos_info = NULL)
{
    TokenList result;

    size_t pos = 0;
    while (pos < line.length())
    {
        while(is_blank(line[pos]) and pos != line.length())
            ++pos;

        size_t token_start = pos;

        Token::Type type = Token::Type::Raw;
        if (line[pos] == '"' or line[pos] == '\'' or line[pos] == '`')
        {
            char delimiter = line[pos];

            if (delimiter == '`')
                type = Token::Type::ShellExpand;

            token_start = ++pos;

            while ((line[pos] != delimiter or line[pos-1] == '\\') and
                    pos != line.length())
                ++pos;

        }
        else
            while (not is_blank(line[pos]) and pos != line.length() and
                   (line[pos] != ';' or line[pos-1] == '\\'))
                ++pos;

        if (token_start != pos)
        {
            if (opt_token_pos_info)
                opt_token_pos_info->push_back({token_start, pos});
            result.push_back({type, line.substr(token_start, pos - token_start)});
        }

        if (line[pos] == ';')
        {
            if (opt_token_pos_info)
                opt_token_pos_info->push_back({pos, pos+1});
            result.push_back(Token{ Token::Type::CommandSeparator });
        }

        ++pos;
    }
    return result;
}

struct command_not_found : runtime_error
{
    command_not_found(const String& command)
        : runtime_error(command + " : no such command") {}
};

void CommandManager::execute(const String& command_line,
                             const Context& context,
                             const EnvVarMap& env_vars)
{
    TokenList tokens = parse(command_line);
    execute(tokens, context, env_vars);
}

static void shell_eval(TokenList& params,
                       const String& cmdline,
                       const Context& context,
                       const EnvVarMap& env_vars)
{
        String output = ShellManager::instance().eval(cmdline, context, env_vars);
        TokenList tokens = parse(output);

        for (auto& token : tokens)
            params.push_back(std::move(token));
}

void CommandManager::execute(const CommandParameters& params,
                             const Context& context,
                             const EnvVarMap& env_vars)
{
    if (params.empty())
        return;

    auto begin = params.begin();
    auto end = begin;
    while (true)
    {
        while (end != params.end() and end->type() != Token::Type::CommandSeparator)
            ++end;

        if (end != begin)
        {
            if (begin->type() != Token::Type::Raw)
                throw command_not_found("unable to parse command name");
            auto command_it = m_commands.find(begin->content());
            if (command_it == m_commands.end())
                throw command_not_found(begin->content());

            if (command_it->second.flags & IgnoreSemiColons)
                end = params.end();

            if (command_it->second.flags & DeferredShellEval)
                command_it->second.command(CommandParameters(begin + 1, end), context);
            else
            {
                TokenList expanded_tokens;
                for (auto param = begin+1; param != end; ++param)
                {
                    if (param->type() == Token::Type::ShellExpand)
                        shell_eval(expanded_tokens, param->content(),
                                   context, env_vars);
                    else
                        expanded_tokens.push_back(*param);
                }
                command_it->second.command(expanded_tokens, context);
            }
        }

        if (end == params.end())
            break;

        begin = end+1;
        end   = begin;
    }
}

Completions CommandManager::complete(const String& command_line, size_t cursor_pos)
{
    TokenPosList pos_info;
    TokenList tokens = parse(command_line, &pos_info);

    size_t token_to_complete = tokens.size();
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (pos_info[i].first <= cursor_pos and pos_info[i].second >= cursor_pos)
        {
            token_to_complete = i;
            break;
        }
    }

    if (token_to_complete == 0 or tokens.empty()) // command name completion
    {
        size_t cmd_start = tokens.empty() ? 0 : pos_info[0].first;
        Completions result(cmd_start, cursor_pos);
        String prefix = command_line.substr(cmd_start,
                                            cursor_pos - cmd_start);

        for (auto& command : m_commands)
        {
            if (command.first.substr(0, prefix.length()) == prefix)
                result.candidates.push_back(command.first);
        }
        std::sort(result.candidates.begin(), result.candidates.end());

        return result;
    }

    assert(not tokens.empty());
    if (tokens[0].type() != Token::Type::Raw)
        return Completions();

    const String& command_name = tokens[0].content();

    auto command_it = m_commands.find(command_name);
    if (command_it == m_commands.end() or not command_it->second.completer)
        return Completions();

    size_t start = token_to_complete < tokens.size() ?
                   pos_info[token_to_complete].first : cursor_pos;
    Completions result(start , cursor_pos);
    size_t cursor_pos_in_token = cursor_pos - start;

    CommandParameters params(tokens.begin()+1, tokens.end());
    result.candidates = command_it->second.completer(params,
                                                     token_to_complete - 1,
                                                     cursor_pos_in_token);
    return result;
}

CandidateList PerArgumentCommandCompleter::operator()(const CommandParameters& params,
                                                      size_t token_to_complete,
                                                      size_t pos_in_token) const
{
    if (token_to_complete >= m_completers.size() or
        (token_to_complete < params.size() and
         params[token_to_complete].type() != Token::Type::Raw))
        return CandidateList();

    // it is possible to try to complete a new argument
    assert(token_to_complete <= params.size());

    const String& argument = token_to_complete < params.size() ?
                             params[token_to_complete].content() : String();
    return m_completers[token_to_complete](argument, pos_in_token);
}

}
