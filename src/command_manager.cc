#include "command_manager.hh"

#include "alias_registry.hh"
#include "assert.hh"
#include "buffer_utils.hh"
#include "context.hh"
#include "flags.hh"
#include "file.hh"
#include "optional.hh"
#include "option_types.hh"
#include "ranges.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "shell_manager.hh"
#include "utils.hh"
#include "unit_tests.hh"

#include <algorithm>

namespace Kakoune
{

bool CommandManager::command_defined(StringView command_name) const
{
    return m_commands.find(command_name) != m_commands.end();
}

void CommandManager::register_command(String command_name,
                                      CommandFunc func,
                                      String docstring,
                                      ParameterDesc param_desc,
                                      CommandFlags flags,
                                      CommandHelper helper,
                                      CommandCompleter completer)
{
    m_commands[command_name] = { std::move(func),
                                 std::move(docstring),
                                 std::move(param_desc),
                                 flags,
                                 std::move(helper),
                                 std::move(completer) };
}

bool CommandManager::module_defined(StringView module_name) const
{
    return m_modules.find(module_name) != m_modules.end();
}

void CommandManager::register_module(String module_name, String commands)
{
    auto module = m_modules.find(module_name);
    if (module != m_modules.end() and module->value.state != Module::State::Registered)
        throw runtime_error{format("module already loaded: '{}'", module_name)};

    m_modules[module_name] = { Module::State::Registered, std::move(commands) };
}

void CommandManager::load_module(StringView module_name, Context& context)
{
    auto module = m_modules.find(module_name);
    if (module == m_modules.end())
        throw runtime_error{format("no such module: '{}'", module_name)};
    switch (module->value.state)
    {
        case Module::State::Loading:
            throw runtime_error(format("module '{}' loaded recursively", module_name));
        case Module::State::Loaded: return;
        case Module::State::Registered: default: break;
    }

    {
        module->value.state = Module::State::Loading;
        auto restore_state = on_scope_end([&] { module->value.state = Module::State::Registered; });

        Context empty_context{Context::EmptyContextFlag{}};
        execute(module->value.commands, empty_context);
    }
    module->value.commands.clear();
    module->value.state = Module::State::Loaded;

    context.hooks().run_hook(Hook::ModuleLoaded, module_name, context);
}

struct parse_error : runtime_error
{
    parse_error(StringView error)
        : runtime_error{format("parse error: {}", error)} {}
};

Codepoint Reader::operator*() const
{
    kak_assert(pos < str.end());
    return utf8::codepoint(pos, str.end());
}

Codepoint Reader::peek_next() const
{
    return utf8::codepoint(utf8::next(pos, str.end()), str.end());
}

Reader& Reader::operator++()
{
    kak_assert(pos < str.end());
    if (*pos == '\n')
    {
        ++line;
        line_start = ++pos;
        return *this;
    }
    utf8::to_next(pos, str.end());
    return *this;
}

Reader& Reader::next_byte()
{
    kak_assert(pos < str.end());
    if (*pos++ == '\n')
    {
        ++line;
        line_start = pos;
    }
    return *this;
}

namespace
{

bool is_command_separator(Codepoint c)
{
    return c == ';' or c == '\n';
}

struct QuotedResult
{
    String content;
    bool terminated;
};

QuotedResult parse_quoted(Reader& reader, Codepoint delimiter)
{
    auto beg = reader.pos;
    String str;

    while (reader)
    {
        const Codepoint c = *reader;
        if (c == delimiter)
        {
            if (reader.peek_next() != delimiter)
            {
                str += reader.substr_from(beg);
                ++reader;
                return {str, true};
            }
            str += (++reader).substr_from(beg);
            beg = reader.pos+1;
        }
        ++reader;
    }
    if (beg < reader.str.end())
        str += reader.substr_from(beg);
    return {str, false};
}

QuotedResult parse_quoted_balanced(Reader& reader, char opening_delimiter,
                                   char closing_delimiter)
{
    kak_assert(utf8::codepoint(utf8::previous(reader.pos, reader.str.begin()),
                               reader.str.end()) == opening_delimiter);
    int level = 0;
    auto start = reader.pos;
    while (reader)
    {
        const char c = *reader.pos;
        if (c == opening_delimiter)
            ++level;
        else if (c == closing_delimiter and level-- == 0)
        {
            auto content = reader.substr_from(start);
            reader.next_byte();
            return {String{String::NoCopy{}, content}, true};
        }
        reader.next_byte();
    }
    return {String{String::NoCopy{}, reader.substr_from(start)}, false};
}

String parse_unquoted(Reader& reader)
{
    auto beg = reader.pos;
    String str;

    while (reader)
    {
        const char c = *reader.pos;
        if (is_command_separator(c) or is_horizontal_blank(c))
        {
            str += reader.substr_from(beg);
            if (reader.pos != reader.str.begin() and *(reader.pos - 1) == '\\')
            {
                str.back() = c;
                beg = reader.pos+1;
            }
            else
                return str;
        }
        reader.next_byte();
    }
    if (beg < reader.str.end())
        str += reader.substr_from(beg);
    return str;
}

Token::Type token_type(StringView type_name, bool throw_on_invalid)
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
    else if (type_name == "file")
        return Token::Type::FileExpand;
    else if (throw_on_invalid)
        throw parse_error{format("unknown expand '{}'", type_name)};
    else
        return Token::Type::RawQuoted;
}

void skip_blanks_and_comments(Reader& reader)
{
    while (reader)
    {
        const Codepoint c = *reader.pos;
        if (is_horizontal_blank(c))
            reader.next_byte();
        else if (c == '\\' and reader.pos + 1 != reader.str.end() and
                 *(reader.pos + 1) == '\n')
            reader.next_byte().next_byte();
        else if (c == '#')
        {
            while (reader and *reader != '\n')
                reader.next_byte();
        }
        else
            break;
    }
}

Token parse_percent_token(Reader& reader, bool throw_on_unterminated)
{
    kak_assert(*reader == '%');
    ++reader;

    const auto type_start = reader.pos;
    while (reader and iswalpha(*reader))
        ++reader;
    StringView type_name = reader.substr_from(type_start);

    if (not reader or is_blank(*reader))
    {
        if (throw_on_unterminated)
            throw parse_error{format("expected a string delimiter after '%{}'",
                                     type_name)};
        return {};
    }

    Token::Type type = token_type(type_name, throw_on_unterminated);

    constexpr struct CharPair { Codepoint opening; Codepoint closing; } matching_pairs[] = {
        { '(', ')' }, { '[', ']' }, { '{', '}' }, { '<', '>' }
    };

    const Codepoint opening_delimiter = *reader;
    auto coord = reader.coord();
    ++reader;
    auto start = reader.pos;

    auto it = find_if(matching_pairs, [opening_delimiter](const CharPair& cp)
                      { return opening_delimiter == cp.opening; });

    const auto str_beg = reader.str.begin();
    if (it != std::end(matching_pairs))
    {
        const Codepoint closing_delimiter = it->closing;
        auto quoted = parse_quoted_balanced(reader, opening_delimiter, closing_delimiter);
        if (throw_on_unterminated and not quoted.terminated)
            throw parse_error{format("{}:{}: unterminated string '%{}{}...{}'",
                                     coord.line, coord.column, type_name,
                                     opening_delimiter, closing_delimiter)};

        return {type, start - str_beg, coord, std::move(quoted.content)};
    }
    else
    {
        auto quoted = parse_quoted(reader, opening_delimiter);

        if (throw_on_unterminated and not quoted.terminated)
            throw parse_error{format("{}:{}: unterminated string '%{}{}...{}'",
                                     coord.line, coord.column, type_name,
                                     opening_delimiter, opening_delimiter)};

        return {type, start - str_beg, coord, std::move(quoted.content)};
    }
}

template<bool single>
std::conditional_t<single, String, Vector<String>>
expand_token(const Token& token, const Context& context, const ShellContext& shell_context)
{
    auto& content = token.content;
    switch (token.type)
    {
    case Token::Type::ShellExpand:
    {
        auto str = ShellManager::instance().eval(
            content, context, {}, ShellManager::Flags::WaitForStdout,
            shell_context).first;

        if (not str.empty() and str.back() == '\n')
            str.resize(str.length() - 1, 0);

        return {str};
    }
    case Token::Type::RegisterExpand:
        if constexpr (single)
            return context.main_sel_register_value(content).str();
        else
            return RegisterManager::instance()[content].get(context) | gather<Vector<String>>();
    case Token::Type::OptionExpand:
    {
        auto& opt = context.options()[content];
        if constexpr (single)
            return opt.get_as_string(Quoting::Raw);
        else
            return opt.get_as_strings();
    }
    case Token::Type::ValExpand:
    {
        auto it = shell_context.env_vars.find(content);
        if (it != shell_context.env_vars.end())
            return {it->value};

        auto val = ShellManager::instance().get_val(content, context);
        if constexpr (single)
            return join(val, false, ' ');
        else
            return val;
    }
    case Token::Type::ArgExpand:
    {
        auto& params = shell_context.params;
        if (content == '@')
        {
            if constexpr (single)
                return join(params, ' ', false);
            else
                return Vector<String>{params.begin(), params.end()};
        }

        const int arg = str_to_int(content)-1;
        if (arg < 0)
            throw runtime_error("invalid argument index");
        return {arg < params.size() ? params[arg] : String{}};
    }
    case Token::Type::FileExpand:
        return {read_file(content)};
    case Token::Type::RawEval:
        return {expand(content, context, shell_context)};
    case Token::Type::Raw:
    case Token::Type::RawQuoted:
        return {content};
    default: kak_assert(false);
    }
    return {};
}

}

CommandParser::CommandParser(StringView command_line) : m_reader{command_line} {}

Optional<Token> CommandParser::read_token(bool throw_on_unterminated)
{
    skip_blanks_and_comments(m_reader);
    if (not m_reader)
        return {};

    const StringView line = m_reader.str;
    const char* start = m_reader.pos;
    auto coord = m_reader.coord();

    const char c = *m_reader.pos;
    if (c == '"' or c == '\'')
    {
        start = m_reader.next_byte().pos;
        QuotedResult quoted = parse_quoted(m_reader, c);
        if (throw_on_unterminated and not quoted.terminated)
            throw parse_error{format("unterminated string {0}...{0}", c)};
        return Token{c == '"' ? Token::Type::RawEval
                              : Token::Type::RawQuoted,
                     start - line.begin(), coord, std::move(quoted.content)};
    }
    else if (c == '%')
    {
        auto token = parse_percent_token(m_reader, throw_on_unterminated);
        return token;
    }
    else if (is_command_separator(c))
    {
        m_reader.next_byte();
        return Token{Token::Type::CommandSeparator,
                     m_reader.pos - line.begin(), coord, {}};
    }
    else
    {
        if (c == '\\')
        {
            auto next = m_reader.peek_next();
            if (next == '%' or next == '\'' or next == '"')
                m_reader.next_byte();
        }
        return Token{Token::Type::Raw, start - line.begin(),
                     coord, parse_unquoted(m_reader)};
    }
    return {};
}

template<typename Postprocess>
String expand_impl(StringView str, const Context& context,
                   const ShellContext& shell_context,
                   Postprocess postprocess)
{
    Reader reader{str};
    String res;
    auto beg = str.begin();
    while (reader)
    {
        Codepoint c = *reader;
        if (c == '%')
        {
            if (reader.peek_next() == '%')
            {
                res += (++reader).substr_from(beg);
                beg = (++reader).pos;
            }
            else
            {
                res += reader.substr_from(beg);
                res += postprocess(expand_token<true>(parse_percent_token(reader, true), context, shell_context));
                beg = reader.pos;
            }
        }
        else
            ++reader;
    }
    res += reader.substr_from(beg);
    return res;
}

String expand(StringView str, const Context& context,
              const ShellContext& shell_context)
{
    return expand_impl(str, context, shell_context, [](String s){ return s; });
}

String expand(StringView str, const Context& context,
              const ShellContext& shell_context,
              const FunctionRef<String (String)>& postprocess)
{
    return expand_impl(str, context, shell_context, postprocess);
}

struct command_not_found : runtime_error
{
    command_not_found(StringView name)
        : runtime_error(format("no such command: '{}'", name)) {}
};

StringView resolve_alias(const Context& context, StringView name)
{
    auto alias = context.aliases()[name];
    return alias.empty() ? name : alias;
}

void CommandManager::execute_single_command(CommandParameters params,
                                            Context& context,
                                            const ShellContext& shell_context,
                                            BufferCoord pos)
{
    if (params.empty())
        return;

    constexpr int max_command_depth = 100;
    if (m_command_depth > max_command_depth)
        throw runtime_error("maximum nested command depth hit");

    ++m_command_depth;
    auto pop_cmd = on_scope_end([this] { --m_command_depth; });

    ParameterList param_view(params.begin()+1, params.end());
    auto command_it = m_commands.find(resolve_alias(context, params[0]));
    if (command_it == m_commands.end())
        throw command_not_found(params[0]);

    auto debug_flags = context.options()["debug"].get<DebugFlags>();
    auto start = (debug_flags & DebugFlags::Profile) ? Clock::now() : Clock::time_point{};
    if (debug_flags & DebugFlags::Commands)
        write_to_debug_buffer(format("command {}", join(params, ' ')));

    on_scope_end([&] {
        if (not (debug_flags & DebugFlags::Profile))
            return;
        auto full = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
        write_to_debug_buffer(format("command {} took {} us", params[0], full.count()));
    });

    try
    {
        ParametersParser parameter_parser(param_view,
                                          command_it->value.param_desc);
        command_it->value.func(parameter_parser, context, shell_context);
    }
    catch (failure& error)
    {
        throw;
    }
    catch (runtime_error& error)
    {
        error.set_what(format("{}:{}: '{}' {}", pos.line+1, pos.column+1,
                              params[0], error.what()));
        throw;
    }
}

void CommandManager::execute(StringView command_line,
                             Context& context, const ShellContext& shell_context)
{
    CommandParser parser(command_line);

    BufferCoord command_coord;
    Vector<String, MemoryDomain::Commands> params;
    while (Optional<Token> token_opt = parser.read_token(true))
    {
        auto& token = *token_opt;
        if (params.empty())
            command_coord = token.coord;

        if (token.type == Token::Type::CommandSeparator)
        {
            execute_single_command(params, context, shell_context, command_coord);
            params.clear();
        }
        else if (token.type == Token::Type::ArgExpand and token.content == '@')
            params.insert(params.end(), shell_context.params.begin(),
                          shell_context.params.end());
        else
        {
            auto tokens = expand_token<false>(token, context, shell_context);
            params.insert(params.end(),
                          std::make_move_iterator(tokens.begin()),
                          std::make_move_iterator(tokens.end()));
        }
    }
    execute_single_command(params, context, shell_context, command_coord);
}

Optional<CommandInfo> CommandManager::command_info(const Context& context, StringView command_line) const
{
    CommandParser parser{command_line};
    Vector<Token> tokens;
    while (auto token = parser.read_token(false))
    {
        if (token->type == Token::Type::CommandSeparator)
            tokens.clear();
        else
            tokens.push_back(std::move(*token));
    }

    if (tokens.empty() or
        (tokens.front().type != Token::Type::Raw and
         tokens.front().type != Token::Type::RawQuoted))
        return {};

    auto cmd = m_commands.find(resolve_alias(context, tokens.front().content));
    if (cmd == m_commands.end())
        return {};

    CommandInfo res;
    res.name = cmd->key;
    if (not cmd->value.docstring.empty())
        res.info += cmd->value.docstring + "\n";

    if (cmd->value.helper)
    {
        Vector<String> params;
        for (auto it = tokens.begin() + 1; it != tokens.end(); ++it)
        {
            if (it->type == Token::Type::Raw or
                it->type == Token::Type::RawQuoted or
                it->type == Token::Type::RawEval)
                params.push_back(it->content);
        }
        String helpstr = cmd->value.helper(context, params);
        if (not helpstr.empty())
            res.info += format("{}\n", helpstr);
    }

    String aliases;
    for (auto& alias : context.aliases().aliases_for(cmd->key))
        aliases += " " + alias;
    if (not aliases.empty())
        res.info += format("Aliases:{}\n", aliases);

    auto& switches = cmd->value.param_desc.switches;
    if (not switches.empty())
        res.info += format("Switches:\n{}", indent(generate_switches_doc(switches)));

    return res;
}

Completions CommandManager::complete_command_name(const Context& context, StringView query) const
{
    auto commands = m_commands
            | filter([](const CommandMap::Item& cmd) { return not (cmd.value.flags & CommandFlags::Hidden); })
            | transform(&CommandMap::Item::key);

    auto aliases = context.aliases().flatten_aliases()
            | transform(&HashItem<String, String>::key);

    return {0, query.length(),
            Kakoune::complete(query, query.length(), concatenated(commands, aliases)),
            Completions::Flags::Menu | Completions::Flags::NoEmpty};
}

Completions CommandManager::complete_module_name(StringView query) const
{
    return {0, query.length(),
            Kakoune::complete(query, query.length(), m_modules | filter([](auto&& item) { return item.value.state == Module::State::Registered; })
                                                               | transform(&ModuleMap::Item::key))};
}

Completions CommandManager::complete(const Context& context,
                                     CompletionFlags flags,
                                     StringView command_line,
                                     ByteCount cursor_pos)
{
    CommandParser parser{command_line};
    const char* cursor = command_line.begin() + (int)cursor_pos;
    Vector<Token> tokens;

    bool is_last_token = true;
    while (auto token = parser.read_token(false))
    {
        if (token->type == Token::Type::CommandSeparator)
        {
            tokens.clear();
            continue;
        }

        tokens.push_back(std::move(*token));
        if (parser.pos() >= cursor)
        {
            is_last_token = false;
            break;
        }
    }

    if (is_last_token)
        tokens.push_back({Token::Type::Raw, command_line.length(), parser.coord(), {}});
    kak_assert(not tokens.empty());
    const auto& token = tokens.back();

    auto requote = [](Completions completions, Token::Type token_type) {
        if (completions.flags & Completions::Flags::Quoted)
            return completions;

        if (token_type == Token::Type::Raw)
        {
            const bool at_token_start = completions.start == 0;
            for (auto& candidate : completions.candidates)
            {
                const StringView to_escape = ";\n \t";
                if ((at_token_start and candidate.substr(0_byte, 1_byte) == "%") or
                    any_of(candidate, [&](auto c) { return contains(to_escape, c); }))
                    candidate = at_token_start ? quote(candidate) : escape(candidate, to_escape, '\\');
            }
        }
        else if (token_type == Token::Type::RawQuoted)
            completions.flags |= Completions::Flags::Quoted;
        else
            kak_assert(false);

        return completions;
    };

    const ByteCount start = token.pos;
    const ByteCount cursor_pos_in_token = cursor_pos - start;

    // command name completion
    if (tokens.size() == 1 and (token.type == Token::Type::Raw or
                                token.type == Token::Type::RawQuoted))
    {
        StringView query = command_line.substr(start, cursor_pos_in_token);
        return offset_pos(requote(complete_command_name(context, query), token.type), start);
    }

    switch (token.type)
    {
    case Token::Type::RegisterExpand:
        return {start , cursor_pos,
                RegisterManager::instance().complete_register_name(
                    token.content, cursor_pos_in_token) };

    case Token::Type::OptionExpand:
        return {start , cursor_pos,
                GlobalScope::instance().option_registry().complete_option_name(
                    token.content, cursor_pos_in_token) };

    case Token::Type::ShellExpand:
        return offset_pos(shell_complete(context, flags, token.content,
                                         cursor_pos_in_token), start);

    case Token::Type::ValExpand:
        return {start , cursor_pos,
                ShellManager::instance().complete_env_var(
                    token.content, cursor_pos_in_token) };

    case Token::Type::FileExpand:
    {
        const auto& ignored_files = context.options()["ignored_files"].get<Regex>();
        return {start , cursor_pos, complete_filename(
                token.content, ignored_files, cursor_pos_in_token, FilenameFlags::Expand) };
    }

    case Token::Type::Raw:
    case Token::Type::RawQuoted:
    {
        StringView command_name = tokens.front().content;
        if (command_name != m_last_complete_command)
        {
            m_last_complete_command = command_name.str();
            flags |= CompletionFlags::Start;
        }

        auto command_it = m_commands.find(resolve_alias(context, command_name));
        if (command_it == m_commands.end())
            return Completions{};

        if (token.content.substr(0_byte, 1_byte) == "-")
        {
            auto switches = Kakoune::complete(token.content.substr(1_byte), cursor_pos_in_token,
                                              command_it->value.param_desc.switches |
                                              transform(&SwitchMap::Item::key));
            if (not switches.empty())
                return Completions{start+1, cursor_pos, std::move(switches)};
        }
        if (not command_it->value.completer)
            return Completions{};

        Vector<String> params;
        for (auto it = tokens.begin() + 1; it != tokens.end(); ++it)
            params.push_back(it->content);
        return offset_pos(requote(command_it->value.completer(
            context, flags, params, tokens.size() - 2,
            cursor_pos_in_token), token.type), start);
    }
    case Token::Type::RawEval:
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
        return complete_command_name(context, prefix);
    else
    {
        StringView command_name = params[0];
        if (command_name != m_last_complete_command)
        {
            m_last_complete_command = command_name.str();
            flags |= CompletionFlags::Start;
        }

        auto command_it = m_commands.find(resolve_alias(context, command_name));
        if (command_it != m_commands.end() and command_it->value.completer)
            return command_it->value.completer(
                context, flags, params.subrange(1),
                token_to_complete-1, pos_in_token);
    }
    return Completions{};
}

UnitTest test_command_parsing{[]
{
    auto check_quoted = [](StringView str, bool terminated, StringView content)
    {
        Reader reader{str};
        const Codepoint delimiter = *reader;
        auto quoted = parse_quoted(++reader, delimiter);
        kak_assert(quoted.terminated == terminated);
        kak_assert(quoted.content == content);
    };
    check_quoted("'abc'", true, "abc");
    check_quoted("'abc''def", false, "abc'def");
    check_quoted("'abc''def'''", true, "abc'def'");

    auto check_balanced = [](StringView str, Codepoint opening, Codepoint closing, bool terminated, StringView content)
    {
        Reader reader{str};
        auto quoted = parse_quoted_balanced(++reader, opening, closing);
        kak_assert(quoted.terminated == terminated);
        kak_assert(quoted.content == content);
    };
    check_balanced("{abc}", '{', '}', true, "abc");
    check_balanced("{abc{def}}", '{', '}', true, "abc{def}");
    check_balanced("{{abc}{def}", '{', '}', false, "{abc}{def}");

    auto check_unquoted = [](StringView str, StringView content)
    {
        Reader reader{str};
        auto res = parse_unquoted(reader);
        kak_assert(res == content);
    };
    check_unquoted("abc def", "abc");
    check_unquoted("abc; def", "abc");
    check_unquoted("abc\\; def", "abc;");
    check_unquoted("abc\\;\\ def", "abc; def");

    {
        CommandParser parser(R"(foo 'bar' "baz" qux)");
        kak_assert(parser.read_token(false)->content == "foo");
        kak_assert(parser.read_token(false)->content == "bar");
        kak_assert(parser.read_token(false)->content == "baz");
        kak_assert(parser.read_token(false)->content == "qux");
        kak_assert(not parser.read_token(false));
    }
}};

}
