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
                                      CommandFlags flags,
                                      CommandCompleter completer)
{
    m_commands[command_name] = { std::move(command), flags, std::move(completer) };
}

void CommandManager::register_commands(memoryview<String> command_names,
                                       Command command,
                                       CommandFlags flags,
                                       CommandCompleter completer)
{
    kak_assert(not command_names.empty());
    m_commands[command_names[0]] = { std::move(command), flags, completer };
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

static String get_until_delimiter(const String& base, ByteCount& pos, char delimiter)
{
    const ByteCount length = base.length();
    String str;
    while (true)
    {
        char c = base[pos];
        if (c == delimiter)
        {
            if (base[pos-1] != '\\')
                return str;
            str.back() = delimiter;
        }
        else
            str += c;
        if (++pos == length)
            throw unterminated_string(String{delimiter}, String{delimiter});
    }
}

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

        if (line[pos] == '"' or line[pos] == '\'')
        {
            char delimiter = line[pos];

            token_start = ++pos;
            String token = get_until_delimiter(line, pos, delimiter);
            result.emplace_back(Token::Type::Raw, std::move(token));
            if (opt_token_pos_info)
                opt_token_pos_info->push_back({token_start, pos});
        }
        else if (line[pos] == '%')
        {
            ByteCount type_start = ++pos;
            while (isalpha(line[pos]))
                ++pos;
            String type_name = line.substr(type_start, pos - type_start);

            if (pos == length)
                throw parse_error{"expected a string delimiter after '%" + type_name + "'"};

            Token::Type type = Token::Type::Raw;
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
                    if (line[pos] == opening_delimiter)
                        ++level;
                    else if (line[pos] == closing_delimiter)
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
                result.emplace_back(type, line.substr(token_start, pos - token_start));
            }
            else
            {
                String token = get_until_delimiter(line, pos, opening_delimiter);
                result.emplace_back(type, std::move(token));
            }
            if (opt_token_pos_info)
                opt_token_pos_info->push_back({token_start, pos});
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
                if (opt_token_pos_info)
                    opt_token_pos_info->push_back({token_start, pos});
                auto token = line.substr(token_start, pos - token_start);
                static const Regex regex{R"(\\([ \t;\n]))"};
                result.emplace_back(Token::Type::Raw,
                                    boost::regex_replace(token, regex, "\\1"));
            }
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
    command_it->second.command(param_view, context);
}

void CommandManager::execute(const String& command_line,
                             Context& context,
                             memoryview<String> shell_params,
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

Completions CommandManager::complete(const Context& context, CompletionFlags flags,
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
            if (command.second.flags & CommandFlags::Hidden)
                continue;
            if ( prefix_match(command.first, prefix))
                result.candidates.push_back(command.first);
        }
        std::sort(result.candidates.begin(), result.candidates.end());

        return result;
    }

    kak_assert(not tokens.empty());
    if (tokens[0].type() != Token::Type::Raw)
        return Completions();

    const String& command_name = tokens[0].content();

    auto command_it = find_command(command_name);
    if (command_it == m_commands.end() or not command_it->second.completer)
        return Completions();

    ByteCount start = token_to_complete < tokens.size() ?
                      pos_info[token_to_complete].first : cursor_pos;
    Completions result(start , cursor_pos);
    ByteCount cursor_pos_in_token = cursor_pos - start;

    std::vector<String> params;
    for (auto token_it = tokens.begin()+1; token_it != tokens.end(); ++token_it)
        params.push_back(token_it->content());
    result.candidates = command_it->second.completer(context, flags, params,
                                                     token_to_complete - 1,
                                                     cursor_pos_in_token);
    return result;
}

CandidateList PerArgumentCommandCompleter::operator()(const Context& context,
                                                      CompletionFlags flags,
                                                      CommandParameters params,
                                                      size_t token_to_complete,
                                                      ByteCount pos_in_token) const
{
    if (token_to_complete >= m_completers.size())
        return CandidateList();

    // it is possible to try to complete a new argument
    kak_assert(token_to_complete <= params.size());

    const String& argument = token_to_complete < params.size() ?
                             params[token_to_complete] : String();
    return m_completers[token_to_complete](context, flags, argument, pos_in_token);
}

}
