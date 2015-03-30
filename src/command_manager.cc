#include "command_manager.hh"

#include "alias_registry.hh"
#include "assert.hh"
#include "context.hh"
#include "register_manager.hh"
#include "shell_manager.hh"
#include "utils.hh"

#include <algorithm>

namespace Kakoune
{

bool CommandManager::command_defined(const String& command_name) const
{
    return m_commands.find(command_name) != m_commands.end();
}

void CommandManager::register_command(String command_name,
                                      Command command,
                                      String docstring,
                                      ParameterDesc param_desc,
                                      CommandFlags flags,
                                      CommandHelper helper,
                                      CommandCompleter completer)
{
    m_commands[command_name] = { std::move(command),
                                 std::move(docstring),
                                 std::move(param_desc),
                                 flags,
                                 std::move(helper),
                                 std::move(completer) };
}

struct parse_error : runtime_error
{
    parse_error(StringView error)
        : runtime_error{format("parse error: {}", error)} {}
};

namespace
{

struct Token
{
    enum class Type
    {
        Raw,
        RawEval,
        ShellExpand,
        RegisterExpand,
        OptionExpand,
        ValExpand,
        CommandSeparator
    };
    Token() : m_type(Type::Raw) {}

    Token(Type type, ByteCount b, ByteCount e, String str = "")
    : m_type(type), m_begin(b), m_end(e), m_content(str) {}

    Type type() const { return m_type; }
    ByteCount begin() const { return m_begin; }
    ByteCount end() const { return m_end; }
    const String& content() const { return m_content; }

private:
    Type   m_type;
    ByteCount m_begin;
    ByteCount m_end;
    String m_content;
};


using TokenList = Vector<Token>;

bool is_command_separator(char c)
{
    return c == ';' or c == '\n';
}

String get_until_delimiter(StringView base, ByteCount& pos, char delimiter)
{
    const ByteCount length = base.length();
    String str;
    while (pos < length)
    {
        char c = base[pos];
        if (c == delimiter)
        {
            if (base[pos-1] != '\\')
                break;
            str.back() = delimiter;
        }
        else
            str += c;
        ++pos;
    }
    return str;
}

String get_until_delimiter(StringView base, ByteCount& pos,
                           char opening_delimiter, char closing_delimiter)
{
    kak_assert(base[pos-1] == opening_delimiter);
    const ByteCount length = base.length();
    int level = 0;
    ByteCount start = pos;
    while (pos != length)
    {
        if (base[pos] == opening_delimiter)
            ++level;
        else if (base[pos] == closing_delimiter)
        {
            if (level > 0)
                --level;
            else
                break;
        }
        ++pos;
    }
    return base.substr(start, pos - start).str();
}

struct unknown_expand : parse_error
{
    unknown_expand(StringView name)
        : parse_error{format("unknown expand '{}'", name)} {}
};

template<bool throw_on_invalid>
Token::Type token_type(StringView type_name)
{
    if (type_name == "")
        return Token::Type::Raw;
    else if (type_name == "sh")
        return Token::Type::ShellExpand;
    else if (type_name == "reg")
        return Token::Type::RegisterExpand;
    else if (type_name == "opt")
        return Token::Type::OptionExpand;
    else if (type_name == "val")
        return Token::Type::ValExpand;
    else if (throw_on_invalid)
        throw unknown_expand{type_name};
    else
        return Token::Type::Raw;
}

void skip_blanks_and_comments(StringView base, ByteCount& pos)
{
    const ByteCount length = base.length();
    while (pos != length)
    {
        if (is_horizontal_blank(base[pos]))
            ++pos;
        else if (base[pos] == '\\' and pos+1 < length and base[pos+1] == '\n')
            pos += 2;
        else if (base[pos] == '#')
        {
            while (pos != length)
            {
                if (base[pos++] == '\n')
                    break;
            }
        }
        else
            break;
    }
}

template<bool throw_on_unterminated>
Token parse_percent_token(StringView line, ByteCount& pos)
{
    const ByteCount length = line.length();
    const ByteCount type_start = ++pos;
    while (isalpha(line[pos]))
        ++pos;
    StringView type_name = line.substr(type_start, pos - type_start);

    if (throw_on_unterminated and pos == length)
        throw parse_error{format("expected a string delimiter after '%{}'",
                                 type_name)};

    Token::Type type = token_type<throw_on_unterminated>(type_name);
    static const UnorderedMap<char, char> matching_delimiters = {
        { '(', ')' }, { '[', ']' }, { '{', '}' }, { '<', '>' }
    };

    char opening_delimiter = line[pos];
    ByteCount token_start = ++pos;

    auto delim_it = matching_delimiters.find(opening_delimiter);
    if (delim_it != matching_delimiters.end())
    {
        char closing_delimiter = delim_it->second;
        String token = get_until_delimiter(line, pos, opening_delimiter,
                                           closing_delimiter);
        if (throw_on_unterminated and pos == length)
            throw parse_error{format("unterminated string '%{}{}...{}'",
                                     type_name, opening_delimiter,
                                     closing_delimiter)};
        return {type, token_start, pos, std::move(token)};
    }
    else
    {
        String token = get_until_delimiter(line, pos, opening_delimiter);
        return {type, token_start, pos, std::move(token)};
    }
}

template<bool throw_on_unterminated>
TokenList parse(StringView line)
{
    TokenList result;

    const ByteCount length = line.length();
    ByteCount pos = 0;
    while (pos < length)
    {
        skip_blanks_and_comments(line, pos);

        ByteCount token_start = pos;
        ByteCount start_pos = pos;

        if (line[pos] == '"' or line[pos] == '\'')
        {
            char delimiter = line[pos];

            token_start = ++pos;
            String token = get_until_delimiter(line, pos, delimiter);
            if (throw_on_unterminated and pos == length)
                throw parse_error{format("unterminated string {0}...{0}", delimiter)};
            result.emplace_back(delimiter == '"' ? Token::Type::RawEval
                                                 : Token::Type::Raw,
                                token_start, pos, std::move(token));
        }
        else if (line[pos] == '%')
            result.push_back(
                parse_percent_token<throw_on_unterminated>(line, pos));
        else
        {
            while (pos != length and
                   ((not is_command_separator(line[pos]) and
                     not is_horizontal_blank(line[pos]))
                    or (pos != 0 and line[pos-1] == '\\')))
                ++pos;
            if (start_pos != pos)
            {
                result.emplace_back(
                    Token::Type::Raw, token_start, pos,
                    unescape(line.substr(token_start, pos - token_start),
                             " \t;\n", '\\'));
            }
        }

        if (is_command_separator(line[pos]))
            result.emplace_back(Token::Type::CommandSeparator, pos, pos+1);

        ++pos;
    }
    return result;
}

String eval_token(const Token& token, Context& context,
                  ConstArrayView<String> shell_params,
                  const EnvVarMap& env_vars);

String eval(StringView str, Context& context,
            ConstArrayView<String> shell_params,
            const EnvVarMap& env_vars)
{
    String res;
    auto pos = 0_byte;
    auto length = str.length();
    while (pos < length)
    {
        if (str[pos] == '\\')
        {
            char c = str[++pos];
            if (c != '%' and c != '\\')
                res += '\\';
            res += c;
            ++pos;
        }
        else if (str[pos] == '%')
        {
            Token token = parse_percent_token<true>(str, pos);
            res += eval_token(token, context, shell_params, env_vars);
            ++pos;
        }
        else
            res += str[pos++];
    }
    return res;
}

String eval_token(const Token& token, Context& context,
                  ConstArrayView<String> shell_params,
                  const EnvVarMap& env_vars)
{
    auto& content = token.content();
    switch (token.type())
    {
    case Token::Type::ShellExpand:
        return ShellManager::instance().eval(content, context, {},
                                             shell_params, env_vars).first;
    case Token::Type::RegisterExpand:
        return context.main_sel_register_value(content).str();
    case Token::Type::OptionExpand:
        return context.options()[content].get_as_string();
    case Token::Type::ValExpand:
    {
        auto it = env_vars.find(content);
        if (it != env_vars.end())
            return it->second;
        return ShellManager::instance().get_val(content, context);
    }
    case Token::Type::RawEval:
        return eval(content, context, shell_params, env_vars);
    case Token::Type::Raw:
        return content;
    default: kak_assert(false);
    }
    return {};
}

}

struct command_not_found : runtime_error
{
    command_not_found(StringView command)
        : runtime_error(command + " : no such command") {}
};

CommandManager::CommandMap::const_iterator
CommandManager::find_command(const Context& context, const String& name) const
{
    auto alias = context.aliases()[name];
    const String& cmd_name = alias.empty() ? name : alias.str();

    return m_commands.find(cmd_name);
}

void CommandManager::execute_single_command(CommandParameters params,
                                            Context& context,
                                            CharCoord pos) const
{
    if (params.empty())
        return;

    ConstArrayView<String> param_view(params.begin()+1, params.end());
    auto command_it = find_command(context, params[0]);
    if (command_it == m_commands.end())
        throw command_not_found(params[0]);

    try
    {
        ParametersParser parameter_parser(param_view,
                                          command_it->second.param_desc);
        command_it->second.command(parameter_parser, context);
    }
    catch (runtime_error& error)
    {
        throw runtime_error(format("{}:{}: '{}' {}", pos.line+1, pos.column+1,
                                   command_it->first, error.what()));
    }
}

static CharCoord find_coord(StringView str, ByteCount offset)
{
    CharCoord res;
    auto it = str.begin();
    auto line_start = it;
    while (it != str.end() and offset > 0)
    {
        if (*it == '\n')
        {
            line_start = it + 1;
            ++res.line;
        }
        ++it;
        --offset;
    }
    res.column = utf8::distance(line_start, it);
    return res;
}

void CommandManager::execute(StringView command_line,
                             Context& context,
                             ConstArrayView<String> shell_params,
                             const EnvVarMap& env_vars)
{
    TokenList tokens = parse<true>(command_line);
    if (tokens.empty())
        return;

    CharCoord command_coord;
    Vector<String> params;
    for (auto it = tokens.begin(); it != tokens.end(); ++it)
    {
        if (params.empty())
            command_coord = find_coord(command_line, it->begin());

        if (it->type() == Token::Type::CommandSeparator)
        {
            execute_single_command(params, context, command_coord);
            params.clear();
        }
        // Shell expand are retokenized
        else if (it->type() == Token::Type::ShellExpand)
        {
            auto shell_tokens = parse<true>(eval_token(*it, context,
                                                       shell_params,
                                                       env_vars));
            it = tokens.erase(it);
            for (auto& token : shell_tokens)
                it = ++tokens.insert(it, std::move(token));

            if (tokens.empty())
                break;

            it -= shell_tokens.size() + 1;
        }
        else
            params.push_back(eval_token(*it, context, shell_params,
                                        env_vars));
    }
    execute_single_command(params, context, command_coord);
}

CommandInfo CommandManager::command_info(const Context& context, StringView command_line) const
{
    TokenList tokens = parse<false>(command_line);
    size_t cmd_idx = 0;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type() == Token::Type::CommandSeparator)
            cmd_idx = i+1;
    }

    CommandInfo res;
    if (cmd_idx == tokens.size() or
        tokens[cmd_idx].type() != Token::Type::Raw)
        return res;

    auto cmd = find_command(context, tokens[cmd_idx].content());
    if (cmd == m_commands.end())
        return res;

    res.first = cmd->first;
    if (not cmd->second.docstring.empty())
        res.second += cmd->second.docstring + "\n";

    if (cmd->second.helper)
    {
        Vector<String> params;
        for (auto it = tokens.begin() + cmd_idx + 1;
             it != tokens.end() and it->type() != Token::Type::CommandSeparator;
             ++it)
        {
            if (it->type() == Token::Type::Raw or it->type() == Token::Type::RawEval)
                params.push_back(it->content());
        }
        String helpstr = cmd->second.helper(context, params);
        if (not helpstr.empty())
        {
            if (helpstr.back() != '\n')
                helpstr += '\n';
            res.second += helpstr;
        }
    }

    String aliases;
    for (auto& alias : context.aliases().aliases_for(cmd->first))
        aliases += " " + alias;
    if (not aliases.empty())
        res.second += "Aliases:" + aliases + "\n";


    auto& switches = cmd->second.param_desc.switches;
    if (not switches.empty())
    {
        res.second += "Switches:\n";
        res.second += generate_switches_doc(switches);
    }

    return res;
}

Completions CommandManager::complete(const Context& context,
                                     CompletionFlags flags,
                                     StringView command_line,
                                     ByteCount cursor_pos)
{
    TokenList tokens = parse<false>(command_line);

    size_t cmd_idx = 0;
    size_t tok_idx = tokens.size();
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type() == Token::Type::CommandSeparator)
            cmd_idx = i+1;

        if (tokens[i].begin() <= cursor_pos and tokens[i].end() >= cursor_pos)
        {
            tok_idx = i;
            break;
        }
    }

     // command name completion
    if (tokens.empty() or
        (tok_idx == cmd_idx and (tok_idx == tokens.size() or
                                 tokens[tok_idx].type() == Token::Type::Raw)))
    {
        const bool is_end_token = tok_idx == tokens.size();
        ByteCount cmd_start =  is_end_token ? cursor_pos
                                            : tokens[tok_idx].begin();
        Completions result(cmd_start, cursor_pos);
        StringView prefix = command_line.substr(cmd_start,
                                                cursor_pos - cmd_start);

        for (auto& command : m_commands)
        {
            if (command.second.flags & CommandFlags::Hidden)
                continue;
            if ( prefix_match(command.first, prefix))
                result.candidates.push_back(command.first);
        }
        std::sort(result.candidates.begin(), result.candidates.end());
        return result;
    }

    kak_assert(not tokens.empty());

    ByteCount start = tok_idx < tokens.size() ?
                      tokens[tok_idx].begin() : cursor_pos;
    ByteCount cursor_pos_in_token = cursor_pos - start;

    const Token::Type token_type = tok_idx < tokens.size() ?
                                   tokens[tok_idx].type() : Token::Type::Raw;
    switch (token_type)
    {
    case Token::Type::OptionExpand:
    {
        Completions result(start , cursor_pos);
        result.candidates = context.options().complete_option_name(
            tokens[tok_idx].content(), cursor_pos_in_token);
        return result;
    }
    case Token::Type::ShellExpand:
    {
        Completions shell_completions = shell_complete(
            context, flags, tokens[tok_idx].content(), cursor_pos_in_token);
        shell_completions.start += start;
        shell_completions.end += start;
        return shell_completions;
    }
    case Token::Type::Raw:
    {
        if (tokens[cmd_idx].type() != Token::Type::Raw)
            return Completions{};

        const String& command_name = tokens[cmd_idx].content();

        auto command_it = find_command(context, command_name);
        if (command_it == m_commands.end() or
            not command_it->second.completer)
            return Completions();

        Vector<String> params;
        for (auto token_it = tokens.begin() + cmd_idx + 1;
             token_it != tokens.end(); ++token_it)
            params.push_back(token_it->content());
        if (tok_idx == tokens.size())
            params.push_back("");
        Completions completions = command_it->second.completer(
            context, flags, params, tok_idx - cmd_idx - 1,
            cursor_pos_in_token);
        completions.start += start;
        completions.end += start;

        for (auto& candidate : completions.candidates)
            candidate = escape(candidate, " \t;", '\\');

        return completions;
    }
    default:
        break;
    }
    return Completions{};
}

Completions PerArgumentCommandCompleter::operator()(const Context& context,
                                                    CompletionFlags flags,
                                                    CommandParameters params,
                                                    size_t token_to_complete,
                                                    ByteCount pos_in_token)
                                                    const
{
    if (token_to_complete >= m_completers.size())
        return Completions{};

    // it is possible to try to complete a new argument
    kak_assert(token_to_complete <= params.size());

    const String& argument = token_to_complete < params.size() ?
                             params[token_to_complete] : String();
    return m_completers[token_to_complete](context, flags, argument,
                                           pos_in_token);
}

}
