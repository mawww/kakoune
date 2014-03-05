#include "command_manager.hh"

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
    return find_command(command_name) != m_commands.end();
}

void CommandManager::register_command(String command_name,
                                      Command command,
                                      String docstring,
                                      ParameterDesc param_desc,
                                      CommandFlags flags,
                                      CommandCompleter completer)
{
    m_commands[command_name] = { std::move(command), std::move(docstring), std::move(param_desc), flags, std::move(completer) };
}

void CommandManager::register_commands(memoryview<String> command_names,
                                       Command command,
                                       String docstring,
                                       ParameterDesc param_desc,
                                       CommandFlags flags,
                                       CommandCompleter completer)
{
    kak_assert(not command_names.empty());
    m_commands[command_names[0]] = { std::move(command), std::move(docstring), std::move(param_desc), flags, completer };
    for (size_t i = 1; i < command_names.size(); ++i)
        m_aliases[command_names[i]] = command_names[0];
}

struct parse_error : runtime_error
{
    parse_error(const String& error)
        : runtime_error{"parse error: " + error} {}
};

namespace
{

struct Token
{
    enum class Type
    {
        Raw,
        ShellExpand,
        RegisterExpand,
        OptionExpand,
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


using TokenList = std::vector<Token>;
using TokenPosList = std::vector<std::pair<ByteCount, ByteCount>>;

bool is_command_separator(char c)
{
    return c == ';' or c == '\n';
}

struct unterminated_string : parse_error
{
    unterminated_string(const String& open, const String& close, int nest = 0)
        : parse_error{"unterminated string '" + open + "..." + close + "'" +
                      (nest > 0 ? "(nesting: " + to_string(nest) + ")" : "")}
    {}
};

struct unknown_expand : parse_error
{
    unknown_expand(const String& name)
        : parse_error{"unknown expand '" + name + "'"} {}
};

String get_until_delimiter(const String& base, ByteCount& pos, char delimiter)
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

String get_until_delimiter(const String& base, ByteCount& pos,
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
    return base.substr(start, pos - start);
}

template<bool throw_on_invalid>
Token::Type token_type(const String& type_name)
{
    if (type_name == "")
        return Token::Type::Raw;
    else if (type_name == "sh")
        return Token::Type::ShellExpand;
    else if (type_name == "reg")
        return Token::Type::RegisterExpand;
    else if (type_name == "opt")
        return Token::Type::OptionExpand;
    else if (throw_on_invalid)
        throw unknown_expand{type_name};
    else
        return Token::Type::Raw;
}

void skip_blanks_and_comments(const String& base, ByteCount& pos)
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
            while (pos != length and base[pos] != '\n')
                ++pos;
        }
        else
            break;
    }
}

template<bool throw_on_unterminated>
TokenList parse(const String& line)
{
    TokenList result;

    ByteCount length = line.length();
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
                throw unterminated_string(String{delimiter}, String{delimiter});
            result.emplace_back(Token::Type::Raw, token_start, pos, std::move(token));
        }
        else if (line[pos] == '%')
        {
            ByteCount type_start = ++pos;
            while (isalpha(line[pos]))
                ++pos;
            String type_name = line.substr(type_start, pos - type_start);

            if (throw_on_unterminated and pos == length)
                throw parse_error{"expected a string delimiter after '%" + type_name + "'"};

            Token::Type type = token_type<throw_on_unterminated>(type_name);
            static const std::unordered_map<char, char> matching_delimiters = {
                { '(', ')' }, { '[', ']' }, { '{', '}' }, { '<', '>' }
            };

            char opening_delimiter = line[pos];
            token_start = ++pos;

            auto delim_it = matching_delimiters.find(opening_delimiter);
            if (delim_it != matching_delimiters.end())
            {
                char closing_delimiter = delim_it->second;
                String token = get_until_delimiter(line, pos, opening_delimiter, closing_delimiter);
                if (throw_on_unterminated and pos == length)
                    throw unterminated_string("%" + type_name + opening_delimiter,
                                              String{closing_delimiter}, 0);
                result.emplace_back(type, token_start, pos, std::move(token));
            }
            else
            {
                String token = get_until_delimiter(line, pos, opening_delimiter);
                result.emplace_back(type, token_start, pos, std::move(token));
            }
        }
        else
        {
            while (pos != length and
                   ((not is_command_separator(line[pos]) and
                     not is_horizontal_blank(line[pos]))
                    or (pos != 0 and line[pos-1] == '\\')))
                ++pos;
            if (start_pos != pos)
            {
                auto token = line.substr(token_start, pos - token_start);
                static const Regex regex{R"(\\([ \t;\n]))"};
                result.emplace_back(Token::Type::Raw, token_start, pos,
                                    boost::regex_replace(token, regex, "\\1"));
            }
        }

        if (is_command_separator(line[pos]))
            result.push_back(Token{ Token::Type::CommandSeparator, pos, pos+1 });

        ++pos;
    }
    return result;
}

}

struct command_not_found : runtime_error
{
    command_not_found(const String& command)
        : runtime_error(command + " : no such command") {}
};

CommandManager::CommandMap::const_iterator CommandManager::find_command(const String& name) const
{
    auto it = m_aliases.find(name);
    const String& cmd_name = it == m_aliases.end() ? name : it->second;

    return m_commands.find(cmd_name);
}

void CommandManager::execute_single_command(CommandParameters params,
                                            Context& context) const
{
    if (params.empty())
        return;

    memoryview<String> param_view(params.begin()+1, params.end());
    auto command_it = find_command(params[0]);
    if (command_it == m_commands.end())
        throw command_not_found(params[0]);
    command_it->second.command(ParametersParser(param_view, command_it->second.param_desc), context);
}

void CommandManager::execute(const String& command_line,
                             Context& context,
                             memoryview<String> shell_params,
                             const EnvVarMap& env_vars)
{
    TokenList tokens = parse<true>(command_line);
    if (tokens.empty())
        return;

    std::vector<String> params;
    for (auto it = tokens.begin(); it != tokens.end(); ++it)
    {
        if (it->type() == Token::Type::ShellExpand)
        {
            String output = ShellManager::instance().eval(it->content(),
                                                          context, shell_params,
                                                          env_vars);
            TokenList shell_tokens = parse<true>(output);
            it = tokens.erase(it);
            for (auto& token : shell_tokens)
                it = ++tokens.insert(it, std::move(token));
            it -= shell_tokens.size();

            // when last token is a ShellExpand which produces no output
            if (it == tokens.end())
                break;
        }
        if (it->type() == Token::Type::RegisterExpand)
        {
            if (it->content().length() != 1)
                throw runtime_error("wrong register name: " + it->content());
            Register& reg = RegisterManager::instance()[it->content()[0]];
            params.push_back(reg.values(context)[0]);
        }
        if (it->type() == Token::Type::OptionExpand)
        {
            const Option& option = context.options()[it->content()];
            params.push_back(option.get_as_string());
        }
        if (it->type() == Token::Type::CommandSeparator)
        {
            execute_single_command(params, context);
            params.clear();
        }
        if (it->type() == Token::Type::Raw)
            params.push_back(it->content());
    }
    execute_single_command(params, context);
}

std::pair<String, String> CommandManager::command_info(const String& command_line) const
{
    TokenList tokens = parse<false>(command_line);
    size_t cmd_idx = 0;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (tokens[i].type() == Token::Type::CommandSeparator)
            cmd_idx = i+1;
    }

    std::pair<String, String> res;
    if (cmd_idx == tokens.size() or tokens[cmd_idx].type() != Token::Type::Raw)
        return res;

    auto cmd = find_command(tokens[cmd_idx].content());
    if (cmd == m_commands.end())
        return res;

    res.first = cmd->first;
    if (not cmd->second.docstring.empty())
        res.second += cmd->second.docstring + "\n";
    auto& switches = cmd->second.param_desc.switches;
    if (not switches.empty())
    {
        res.second += "Switches:\n";
        res.second += generate_switches_doc(switches);
    }

    return res;
}

Completions CommandManager::complete(const Context& context, CompletionFlags flags,
                                     const String& command_line, ByteCount cursor_pos)
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
        ByteCount cmd_start = tok_idx == tokens.size() ? cursor_pos
                                                       : tokens[tok_idx].begin();
        Completions result(cmd_start, cursor_pos);
        String prefix = command_line.substr(cmd_start,
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
        result.candidates = context.options().complete_option_name(tokens[tok_idx].content(), cursor_pos_in_token);
        return result;
    }
    case Token::Type::ShellExpand:
    {
        Completions shell_completions = shell_complete(context, flags, tokens[tok_idx].content(), cursor_pos_in_token);
        shell_completions.start += start;
        shell_completions.end += start;
        return shell_completions;
    }
    case Token::Type::Raw:
    {
        if (tokens[cmd_idx].type() != Token::Type::Raw)
            return Completions{};

        const String& command_name = tokens[cmd_idx].content();

        auto command_it = find_command(command_name);
        if (command_it == m_commands.end() or not command_it->second.completer)
            return Completions();

        std::vector<String> params;
        for (auto token_it = tokens.begin() + cmd_idx + 1; token_it != tokens.end(); ++token_it)
            params.push_back(token_it->content());
        if (tok_idx == tokens.size())
            params.push_back("");
        Completions completions = command_it->second.completer(context, flags, params,
                                                               tok_idx - cmd_idx - 1,
                                                               cursor_pos_in_token);
        completions.start += start;
        completions.end += start;
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
                                                    ByteCount pos_in_token) const
{
    if (token_to_complete >= m_completers.size())
        return Completions{};

    // it is possible to try to complete a new argument
    kak_assert(token_to_complete <= params.size());

    const String& argument = token_to_complete < params.size() ?
                             params[token_to_complete] : String();
    return m_completers[token_to_complete](context, flags, argument, pos_in_token);
}

}
