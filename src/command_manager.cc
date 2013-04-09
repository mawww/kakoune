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
    return m_commands.find(command_name) != m_commands.end();
}

void CommandManager::register_command(String command_name,
                                      Command command,
                                      CommandCompleter completer)
{
    m_commands[command_name] = { std::move(command), std::move(completer) };
}

void CommandManager::register_commands(const memoryview<String>& command_names,
                                       Command command,
                                       CommandCompleter completer)
{
    for (auto command_name : command_names)
        m_commands[command_name] = { command, completer };
}

parse_error::parse_error(const String& error)
    : runtime_error{"parse error: " + error} {}


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

    explicit Token(const String& string) : m_content(string), m_type(Type::Raw) {}
    explicit Token(Type type) : m_type(type) {}
    Token(Type type, String str) : m_content(str), m_type(type) {}

    Type type() const { return m_type; }

    const String& content() const { return m_content; }

private:
    Type   m_type;
    String m_content;
};


using TokenList = std::vector<Token>;
using TokenPosList = std::vector<std::pair<ByteCount, ByteCount>>;

bool is_command_separator(char c)
{
    return c == ';' or c == '\n';
}

bool is_horizontal_blank(char c)
{
   return c == ' ' or c == '\t';
}

struct unterminated_string : parse_error
{
    unterminated_string(const String& open, const String& close, int nest = 0)
        : parse_error{"unterminated string '" + open + "..." + close + "'" +
                      (nest > 0 ? "(nesting: " + int_to_str(nest) + ")" : "")}
    {}
};

struct unknown_expand : parse_error
{
    unknown_expand(const String& name)
        : parse_error{"unknown expand '" + name + "'"} {}
};

TokenList parse(const String& line,
                TokenPosList* opt_token_pos_info = nullptr)
{
    TokenList result;

    ByteCount length = line.length();
    ByteCount pos = 0;
    while (pos < length)
    {
        while (pos != length)
        {
            if (is_horizontal_blank(line[pos]))
                ++pos;
            else if (line[pos] == '\\' and pos+1 < length and line[pos+1] == '\n')
                pos += 2;
            else if (line[pos] == '#')
            {
                while (pos != length and line[pos] != '\n')
                    ++pos;
            }
            else
                break;
        }

        ByteCount token_start = pos;
        ByteCount start_pos = pos;

        Token::Type type = Token::Type::Raw;
        if (line[pos] == '"' or line[pos] == '\'')
        {
            char delimiter = line[pos];

            token_start = ++pos;

            while ((line[pos] != delimiter or line[pos-1] == '\\') and
                    pos != length)
                ++pos;
            if (pos == length)
                throw unterminated_string(String{delimiter}, String{delimiter});
        }
        else if (line[pos] == '%')
        {
            ByteCount type_start = ++pos;
            while (isalpha(line[pos]))
                ++pos;
            String type_name = line.substr(type_start, pos - type_start);

            if (pos == length)
                throw parse_error{"expected a string delimiter after '%" + type_name + "'"};

            if (type_name == "sh")
                type = Token::Type::ShellExpand;
            else if (type_name == "reg")
                type = Token::Type::RegisterExpand;
            else if (type_name == "opt")
                type = Token::Type::OptionExpand;
            else if (type_name != "")
                throw unknown_expand{type_name};

            static const std::unordered_map<char, char> matching_delimiters = {
                { '(', ')' }, { '[', ']' }, { '{', '}' }, { '<', '>' }
            };

            char opening_delimiter = line[pos];
            token_start = ++pos;

            auto delim_it = matching_delimiters.find(opening_delimiter);
            if (delim_it != matching_delimiters.end())
            {
                char closing_delimiter = delim_it->second;
                int level = 0;
                while (pos != length)
                {
                    if (line[pos-1] != '\\' and line[pos] == opening_delimiter)
                        ++level;
                    if (line[pos-1] != '\\' and line[pos] == closing_delimiter)
                    {
                        if (level > 0)
                            --level;
                        else
                            break;
                    }
                    ++pos;
                }
                if (pos == length)
                    throw unterminated_string("%" + type_name + opening_delimiter,
                                              String{closing_delimiter}, level);
            }
            else
            {
                while (pos != length and
                       (line[pos] != opening_delimiter or line[pos-1] == '\\'))
                    ++pos;
                if (pos == length)
                    throw unterminated_string("%" + type_name + opening_delimiter,
                                              String{opening_delimiter});
            }
        }
        else
            while (pos != length and
                   ((not is_command_separator(line[pos]) and
                     not is_horizontal_blank(line[pos]))
                    or (pos != 0 and line[pos-1] == '\\')))
                ++pos;

        if (start_pos != pos)
        {
            if (opt_token_pos_info)
                opt_token_pos_info->push_back({token_start, pos});
            String token = line.substr(token_start, pos - token_start);
            token = token.replace(R"(\\([ \t;\n]))", "\\1");
            result.push_back({type, token});
        }

        if (is_command_separator(line[pos]))
        {
            if (opt_token_pos_info)
                opt_token_pos_info->push_back({pos, pos+1});
            result.push_back(Token{ Token::Type::CommandSeparator });
        }

        ++pos;
    }
    if (not result.empty() and result.back().type() == Token::Type::CommandSeparator)
        result.pop_back();

    return result;
}

}

struct command_not_found : runtime_error
{
    command_not_found(const String& command)
        : runtime_error(command + " : no such command") {}
};

void CommandManager::execute_single_command(const CommandParameters& params,
                                            Context& context) const
{
    if (params.empty())
        return;

    auto command_it = m_commands.find(params[0]);
    if (command_it == m_commands.end())
        throw command_not_found(params[0]);
    memoryview<String> param_view(params.begin()+1, params.end());
    command_it->second.command(param_view, context);
}

void CommandManager::execute(const String& command_line,
                             Context& context,
                             const memoryview<String>& shell_params,
                             const EnvVarMap& env_vars)
{
    TokenList tokens = parse(command_line);
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
            TokenList shell_tokens = parse(output);
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

Completions CommandManager::complete(const Context& context,
                                     const String& command_line, ByteCount cursor_pos)
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
        ByteCount cmd_start = tokens.empty() ? 0 : pos_info[0].first;
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

    kak_assert(not tokens.empty());
    if (tokens[0].type() != Token::Type::Raw)
        return Completions();

    const String& command_name = tokens[0].content();

    auto command_it = m_commands.find(command_name);
    if (command_it == m_commands.end() or not command_it->second.completer)
        return Completions();

    ByteCount start = token_to_complete < tokens.size() ?
                   pos_info[token_to_complete].first : cursor_pos;
    Completions result(start , cursor_pos);
    ByteCount cursor_pos_in_token = cursor_pos - start;

    std::vector<String> params;
    for (auto token_it = tokens.begin()+1; token_it != tokens.end(); ++token_it)
        params.push_back(token_it->content());
    result.candidates = command_it->second.completer(context, params,
                                                     token_to_complete - 1,
                                                     cursor_pos_in_token);
    return result;
}

CandidateList PerArgumentCommandCompleter::operator()(const Context& context,
                                                      const CommandParameters& params,
                                                      size_t token_to_complete,
                                                      ByteCount pos_in_token) const
{
    if (token_to_complete >= m_completers.size())
        return CandidateList();

    // it is possible to try to complete a new argument
    kak_assert(token_to_complete <= params.size());

    const String& argument = token_to_complete < params.size() ?
                             params[token_to_complete] : String();
    return m_completers[token_to_complete](context, argument, pos_in_token);
}

}
