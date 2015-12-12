#include "command_manager.hh"

#include "alias_registry.hh"
#include "assert.hh"
#include "context.hh"
#include "register_manager.hh"
#include "shell_manager.hh"
#include "utils.hh"
#include "optional.hh"

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

struct Reader
{
public:
    [[gnu::always_inline]]
    char operator*() const { return str[pos]; }

    Reader& operator++()
    {
        if (str[pos++] == '\n')
        {
            ++coord.line;
            coord.column = 0;
        }
        else
            ++coord.column;
        return *this;
    }

    [[gnu::always_inline]]
    explicit operator bool() const { return pos < str.length(); }

    [[gnu::always_inline]]
    StringView substr_from(ByteCount start) const
    {
        return str.substr(start, pos - start);
    }

    Optional<char> peek_next() const
    {
        if (pos+1 != str.length())
            return str[pos+1];
        return {};
    }

    StringView str;
    ByteCount pos;
    CharCoord coord;
};

bool is_command_separator(char c)
{
    return c == ';' or c == '\n';
}

template<typename Func>
String get_until_delimiter(Reader& reader, Func is_delimiter)
{
    auto beg = reader.pos;
    String str;
    bool was_antislash = false;

    while (reader)
    {
        const char c = *reader;
        if (is_delimiter(c))
        {
            str += reader.substr_from(beg);
            if (was_antislash)
            {
                str.back() = c;
                beg = reader.pos+1;
            }
            else
                return str;
        }
        was_antislash = c == '\\';
        ++reader;
    }
    if (beg < reader.str.length())
        str += reader.substr_from(beg);
    return str;
}

[[gnu::always_inline]]
inline String get_until_delimiter(Reader& reader, char c)
{
    return get_until_delimiter(reader, [c](char ch) { return c == ch; });
}

StringView get_until_closing_delimiter(Reader& reader, char opening_delimiter,
                                       char closing_delimiter)
{
    kak_assert(reader.str[reader.pos-1] == opening_delimiter);
    int level = 0;
    auto start = reader.pos;
    while (reader)
    {
        const char c = *reader;
        if (c == opening_delimiter)
            ++level;
        else if (c == closing_delimiter)
        {
            if (level > 0)
                --level;
            else
                break;
        }
        ++reader;
    }
    return reader.substr_from(start);
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
        return Token::Type::RawQuoted;
    else if (type_name == "sh")
        return Token::Type::ShellExpand;
    else if (type_name == "reg")
        return Token::Type::RegisterExpand;
    else if (type_name == "opt")
        return Token::Type::OptionExpand;
    else if (type_name == "val")
        return Token::Type::ValExpand;
    else if (type_name == "arg")
        return Token::Type::ArgExpand;
    else if (throw_on_invalid)
        throw unknown_expand{type_name};
    else
        return Token::Type::RawQuoted;
}

void skip_blanks_and_comments(Reader& reader)
{
    while (reader)
    {
        const char c = *reader;
        if (is_horizontal_blank(c))
            ++reader;
        else if (c == '\\' and reader.peek_next().value_or('\0') == '\n')
            ++(++reader);
        else if (c == '#')
        {
            for (bool eol = false; reader and not eol; ++reader)
                eol = *reader == '\n';
        }
        else
            break;
    }
}

template<bool throw_on_unterminated>
Token parse_percent_token(Reader& reader)
{
    ++reader;
    const ByteCount type_start = reader.pos;
    while (reader and isalpha(*reader))
        ++reader;
    StringView type_name = reader.substr_from(type_start);

    if (throw_on_unterminated and not reader)
        throw parse_error{format("expected a string delimiter after '%{}'",
                                 type_name)};

    Token::Type type = token_type<throw_on_unterminated>(type_name);

    constexpr struct CharPair { char opening; char closing; } matching_pairs[] = {
        { '(', ')' }, { '[', ']' }, { '{', '}' }, { '<', '>' }
    };

    char opening_delimiter = *reader;
    auto coord = reader.coord;
    ++reader;
    auto start = reader.pos;

    auto it = find_if(matching_pairs, [opening_delimiter](const CharPair& cp)
                      { return opening_delimiter == cp.opening; });

    if (it != std::end(matching_pairs))
    {
        const char closing_delimiter = it->closing;
        auto token = get_until_closing_delimiter(reader, opening_delimiter,
                                                 closing_delimiter);
        if (throw_on_unterminated and not reader)
            throw parse_error{format("{}:{}: unterminated string '%{}{}...{}'",
                                     coord.line, coord.column, type_name,
                                     opening_delimiter, closing_delimiter)};

        return {type, start, reader.pos, coord, token.str()};
    }
    else
    {
        String token = get_until_delimiter(reader, opening_delimiter);

        if (throw_on_unterminated and not reader)
            throw parse_error{format("{}:{}: unterminated string '%{}{}...{}'",
                                     coord.line, coord.column, type_name,
                                     opening_delimiter, opening_delimiter)};

        return {type, start, reader.pos, coord, std::move(token)};
    }
}

String expand_token(const Token& token, const Context& context,
                    const ShellContext& shell_context)
{
    auto& content = token.content();
    switch (token.type())
    {
    case Token::Type::ShellExpand:
        return ShellManager::instance().eval(content, context, {},
                                             ShellManager::Flags::WaitForStdout,
                                             shell_context).first;
    case Token::Type::RegisterExpand:
        return context.main_sel_register_value(content).str();
    case Token::Type::OptionExpand:
        return context.options()[content].get_as_string();
    case Token::Type::ValExpand:
    {
        auto it = shell_context.env_vars.find(content);
        if (it != shell_context.env_vars.end())
            return it->value;
        return ShellManager::instance().get_val(content, context);
    }
    case Token::Type::ArgExpand:
    {
        auto& params = shell_context.params;
        if (content == '@')
            return join(params, ' ');

        const int arg = str_to_int(content)-1;
        if (arg < 0)
            throw runtime_error("invalid argument index");
        return arg < params.size() ? params[arg] : String{};
    }
    case Token::Type::RawEval:
        return expand(content, context, shell_context);
    case Token::Type::Raw:
    case Token::Type::RawQuoted:
        return content;
    default: kak_assert(false);
    }
    return {};
}

}

template<bool throw_on_unterminated>
TokenList parse(StringView line)
{
    TokenList result;

    Reader reader{line};
    while (reader)
    {
        skip_blanks_and_comments(reader);

        ByteCount start = reader.pos;
        auto coord = reader.coord;

        const char c = *reader;
        if (c == '"' or c == '\'')
        {
            start = (++reader).pos;
            String token = get_until_delimiter(reader, c);
            if (throw_on_unterminated and not reader)
                throw parse_error{format("unterminated string {0}...{0}", c)};
            result.emplace_back(c == '"' ? Token::Type::RawEval
                                         : Token::Type::RawQuoted,
                                start, reader.pos, coord, std::move(token));
        }
        else if (c == '%')
            result.push_back(
                parse_percent_token<throw_on_unterminated>(reader));
        else
        {
            String str = get_until_delimiter(reader, [](char c) {
                return is_command_separator(c) or is_horizontal_blank(c);
            });

            if (not str.empty())
                result.emplace_back(Token::Type::Raw, start, reader.pos,
                                    coord, unescape(str, "%", '\\'));
        }

        if (is_command_separator(*reader))
            result.emplace_back(Token::Type::CommandSeparator,
                                reader.pos, reader.pos+1, coord);

        ++reader;
    }
    return result;
}

String expand(StringView str, const Context& context,
              const ShellContext& shell_context)
{
    Reader reader{str};
    String res;
    auto beg = 0_byte;
    while (reader)
    {
        char c = *reader;
        if (c == '\\')
        {
            c = *++reader;
            if (c == '%' or c == '\\')
            {
                res += reader.substr_from(beg);
                res.back() = c;
                beg = (++reader).pos;
            }
        }
        else if (c == '%')
        {
            res += reader.substr_from(beg);
            Token token = parse_percent_token<true>(reader);
            res += expand_token(token, context, shell_context);
            beg = (++reader).pos;
        }
        else
            ++reader;
    }
    res += reader.substr_from(beg);
    return res;
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
                                            const ShellContext& shell_context,
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
        command_it->second.command(parameter_parser, context, shell_context);
    }
    catch (runtime_error& error)
    {
        throw runtime_error(format("{}:{}: '{}' {}", pos.line+1, pos.column+1,
                                   command_it->first, error.what()));
    }
}

void CommandManager::execute(StringView command_line,
                             Context& context, const ShellContext& shell_context)
{
    TokenList tokens = parse<true>(command_line);
    if (tokens.empty())
        return;

    CharCoord command_coord;
    Vector<String> params;
    for (auto it = tokens.begin(); it != tokens.end(); ++it)
    {
        if (params.empty())
            command_coord = it->coord();

        if (it->type() == Token::Type::CommandSeparator)
        {
            execute_single_command(params, context, shell_context, command_coord);
            params.clear();
        }
        // Shell expand are retokenized
        else if (it->type() == Token::Type::ShellExpand)
        {
            auto shell_tokens = parse<true>(expand_token(*it, context,
                                                         shell_context));
            it = tokens.erase(it);
            for (Token& token : shell_tokens)
                it = ++tokens.emplace(it, std::move(token));

            if (tokens.empty())
                break;

            it -= shell_tokens.size() + 1;
        }
        else if (it->type() == Token::Type::ArgExpand and it->content() == '@')
            std::copy(shell_context.params.begin(), shell_context.params.end(),
                      std::back_inserter(params));
        else
            params.push_back(expand_token(*it, context, shell_context));
    }
    execute_single_command(params, context, shell_context, command_coord);
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
        (tokens[cmd_idx].type() != Token::Type::Raw and
         tokens[cmd_idx].type() != Token::Type::RawQuoted))
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
            if (it->type() == Token::Type::Raw or
                it->type() == Token::Type::RawQuoted or
                it->type() == Token::Type::RawEval)
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
                                 tokens[tok_idx].type() == Token::Type::Raw or
                                 tokens[tok_idx].type() == Token::Type::RawQuoted)))
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
            if (prefix_match(command.first, prefix))
                result.candidates.push_back(command.first);
        }
        for (auto& alias : context.aliases().flatten_aliases())
        {
            if (prefix_match(alias.first, prefix))
                result.candidates.push_back(alias.first.str());
        }
        std::sort(result.candidates.begin(), result.candidates.end());
        return result;
    }

    kak_assert(not tokens.empty());

    ByteCount start = tok_idx < tokens.size() ?
                      tokens[tok_idx].begin() : cursor_pos;
    ByteCount cursor_pos_in_token = cursor_pos - start;

    const Token::Type type = tok_idx < tokens.size() ?
                             tokens[tok_idx].type() : Token::Type::Raw;
    switch (type)
    {
    case Token::Type::OptionExpand:
        return {start , cursor_pos,
                GlobalScope::instance().option_registry().complete_option_name(
                    tokens[tok_idx].content(), cursor_pos_in_token) };

    case Token::Type::ShellExpand:
        return offset_pos(shell_complete(context, flags, tokens[tok_idx].content(),
                                         cursor_pos_in_token), start);

    case Token::Type::Raw:
    case Token::Type::RawQuoted:
    case Token::Type::RawEval:
    {
        if (tokens[cmd_idx].type() != Token::Type::Raw)
            return Completions{};

        const String& command_name = tokens[cmd_idx].content();

        auto command_it = find_command(context, command_name);
        if (command_it == m_commands.end() or
            not command_it->second.completer)
            return Completions();

        Vector<String> params;
        for (auto it = tokens.begin() + cmd_idx + 1; it != tokens.end(); ++it)
            params.push_back(it->content());
        if (tok_idx == tokens.size())
            params.push_back("");
        Completions completions = offset_pos(command_it->second.completer(
            context, flags, params, tok_idx - cmd_idx - 1,
            cursor_pos_in_token), start);

        if (type != Token::Type::RawQuoted)
        {
            StringView to_escape = type == Token::Type::Raw ? "% \t;" : "%";
            for (auto& candidate : completions.candidates)
                candidate = escape(candidate, to_escape, '\\');
        }

        return completions;
    }
    default:
        break;
    }
    return Completions{};
}

Completions CommandManager::complete(const Context& context,
                                     CompletionFlags flags,
                                     CommandParameters params,
                                     size_t token_to_complete,
                                     ByteCount pos_in_token)
{
    StringView prefix = params[token_to_complete].substr(0, pos_in_token);

    if (token_to_complete == 0)
    {
        CandidateList candidates;
        for (auto& command : m_commands)
        {
            if (command.second.flags & CommandFlags::Hidden)
                continue;
            if (prefix_match(command.first, prefix))
                candidates.push_back(command.first);
        }
        std::sort(candidates.begin(), candidates.end());
        return {0, pos_in_token, std::move(candidates)};
    }
    else
    {
        const String& command_name = params[0];

        auto command_it = find_command(context, command_name);
        if (command_it != m_commands.end() and command_it->second.completer)
            return command_it->second.completer(
                context, flags, params.subrange(1, params.size()-1),
                token_to_complete-1, pos_in_token);
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
