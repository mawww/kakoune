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

void CommandManager::set_command_completer(StringView command_name, CommandCompleter completer)
{
    auto it = m_commands.find(command_name);
    if (it == m_commands.end())
        throw runtime_error(format("no such command '{}'", command_name));

    it->value.completer = std::move(completer);
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

namespace
{

bool is_command_separator(char c)
{
    return c == ';' or c == '\n';
}

struct ParseResult
{
    String content;
    bool terminated;
};

template<typename Delimiter>
ParseResult parse_quoted(ParseState& state, Delimiter delimiter)
{
    static_assert(std::is_same_v<Delimiter, char> or std::is_same_v<Delimiter, Codepoint>);
    auto read = [](const char*& it, const char* end) {
        if constexpr (std::is_same_v<Delimiter, Codepoint>)
            return utf8::read_codepoint(it, end);
        else
            return *it++;
    };

    const char* beg = state.pos;
    const char* end = state.str.end();
    String str;

    while (state.pos != end)
    {
        const char* cur = state.pos;
        const auto c = read(state.pos, end);
        if (c == delimiter)
        {
            auto next = state.pos;
            if (next == end || read(next, end) != delimiter)
            {
                if (str.empty())
                    return {String{String::NoCopy{}, {beg, cur}}, true};

                str += StringView{beg, cur};
                return {str, true};
            }
            str += StringView{beg, state.pos};
            state.pos = beg = next;
        }
    }
    if (beg < end)
        str += StringView{beg, end};
    return {str, false};
}

template<char opening_delimiter, char closing_delimiter>
ParseResult parse_quoted_balanced(ParseState& state)
{
    int level = 1;
    const char* pos = state.pos;
    const char* beg = pos;
    const char* end = state.str.end();
    while (pos != end)
    {
        const char c = *pos++;
        if (c == opening_delimiter)
            ++level;
        else if (c == closing_delimiter and --level == 0)
            break;
    }
    state.pos = pos;
    const bool terminated = (level == 0);
    return {String{String::NoCopy{}, {beg, pos - terminated}}, terminated};
}

String parse_unquoted(ParseState& state)
{
    const char* beg = state.pos;
    const char* end = state.str.end();

    String str;

    while (state.pos != end)
    {
        const char c = *state.pos;
        if (is_command_separator(c) or is_horizontal_blank(c))
        {
            str += StringView{beg, state.pos};
            if (state.pos != beg and *(state.pos - 1) == '\\')
            {
                str.back() = c;
                beg = state.pos+1;
            }
            else
                return str;
        }
        ++state.pos;
    }
    if (beg < end)
        str += StringView{beg, end};
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

void skip_blanks_and_comments(ParseState& state)
{
    while (state)
    {
        const Codepoint c = *state.pos;
        if (is_horizontal_blank(c))
            ++state.pos;
        else if (c == '\\' and state.pos + 1 != state.str.end() and
                 state.pos[1] == '\n')
            state.pos += 2;
        else if (c == '#')
        {
            while (state and *state.pos != '\n')
                ++state.pos;
        }
        else
            break;
    }
}

BufferCoord compute_coord(StringView s)
{
    BufferCoord coord{0,0};
    for (auto c : s)
    {
        if (c == '\n')
        {
            ++coord.line;
            coord.column = 0;
        }
        else
            ++coord.column;
    }
    return coord;
}

Token parse_percent_token(ParseState& state, bool throw_on_unterminated)
{
    kak_assert(state.pos[-1] == '%');
    const auto type_start = state.pos;
    while (state and *state.pos >= 'a' and *state.pos <= 'z')
        ++state.pos;
    StringView type_name{type_start, state.pos};

    bool at_end = state.pos == state.str.end();
    const Codepoint opening_delimiter = utf8::read_codepoint(state.pos, state.str.end());
    if (at_end or iswalpha(opening_delimiter))
    {
        if (throw_on_unterminated)
            throw parse_error{format("expected a string delimiter after '%{}'",
                                     type_name)};
        return {};
    }

    Token::Type type = token_type(type_name, throw_on_unterminated);

    constexpr struct CharPair { char opening; char closing; ParseResult (*parse_func)(ParseState&); } matching_pairs[] = {
        { '(', ')', parse_quoted_balanced<'(', ')'> },
        { '[', ']', parse_quoted_balanced<'[', ']'> },
        { '{', '}', parse_quoted_balanced<'{', '}'> },
        { '<', '>', parse_quoted_balanced<'<', '>'> }
    };

    auto start = state.pos;
    const ByteCount byte_pos = start - state.str.begin();

    if (auto it = find_if(matching_pairs, [=](const CharPair& cp) { return opening_delimiter == cp.opening; });
        it != std::end(matching_pairs))
    {
        auto quoted = it->parse_func(state);
        if (throw_on_unterminated and not quoted.terminated)
        {
            auto coord = compute_coord({state.str.begin(), start});
            throw parse_error{format("{}:{}: unterminated string '%{}{}...{}'",
                                     coord.line+1, coord.column+1, type_name,
                                     it->opening, it->closing)};
        }

        return {type, byte_pos, std::move(quoted.content), quoted.terminated};
    }
    else
    {
        const bool is_ascii = opening_delimiter < 128;
        auto quoted = is_ascii ? parse_quoted(state, (char)opening_delimiter) : parse_quoted(state, opening_delimiter);

        if (throw_on_unterminated and not quoted.terminated)
        {
            auto coord = compute_coord({state.str.begin(), start});
            throw parse_error{format("{}:{}: unterminated string '%{}{}...{}'",
                                     coord.line+1, coord.column+1, type_name,
                                     opening_delimiter, opening_delimiter)};
        }

        return {type, byte_pos, std::move(quoted.content), quoted.terminated};
    }
}

template<typename Target>
    requires (std::is_same_v<Target, Vector<String>> or std::is_same_v<Target, String>)
void expand_token(Token&& token, const Context& context, const ShellContext& shell_context, Target& target)
{
    constexpr bool single = std::is_same_v<Target, String>;
    auto set_target = [&](auto&& s) {
        if constexpr (single)
            target = std::move(s);
        else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(s)>, String>)
            target.push_back(std::move(s));
        else if constexpr (std::is_same_v<decltype(s), Vector<String>&&>)
            target.insert(target.end(), std::make_move_iterator(s.begin()), std::make_move_iterator(s.end()));
        else
            target.insert(target.end(), s.begin(), s.end());
    };

    auto&& content = token.content;
    switch (token.type)
    {
    case Token::Type::ShellExpand:
    {
        auto str = ShellManager::instance().eval(
            content, context, {}, ShellManager::Flags::WaitForStdout,
            shell_context).first;

        if (not str.empty() and str.back() == '\n')
            str.resize(str.length() - 1, 0);

        return set_target(std::move(str));
    }
    case Token::Type::RegisterExpand:
        if constexpr (single)
            return set_target(context.main_sel_register_value(content).str());
        else
            return set_target(RegisterManager::instance()[content].get(context));
    case Token::Type::OptionExpand:
    {
        auto& opt = context.options()[content];
        if constexpr (single)
            return set_target(opt.get_as_string(Quoting::Raw));
        else
            return set_target(opt.get_as_strings());
    }
    case Token::Type::ValExpand:
    {
        auto it = shell_context.env_vars.find(content);
        if (it != shell_context.env_vars.end())
            return set_target(it->value);

        auto val = ShellManager::instance().get_val(content, context);
        if constexpr (single)
            return set_target(join(val, false, ' '));
        else
            return set_target(std::move(val));
    }
    case Token::Type::ArgExpand:
    {
        auto& params = shell_context.params;
        if (content == '@')
        {
            if constexpr (single)
                return set_target(join(params, ' ', false));
            else
                return set_target(params);
        }

        const int arg = str_to_int(content);
        if (arg < 1)
            throw runtime_error("invalid argument index");
        return set_target(arg <= params.size() ? params[arg-1] : String{});
    }
    case Token::Type::FileExpand:
        return set_target(read_file(content));
    case Token::Type::RawEval:
        return set_target(expand(content, context, shell_context));
    case Token::Type::Raw:
    case Token::Type::RawQuoted:
        return set_target(std::move(content));
    default: kak_assert(false);
    }
}

}

CommandParser::CommandParser(StringView command_line) : m_state{command_line, command_line.begin()} {}

Optional<Token> CommandParser::read_token(bool throw_on_unterminated)
{
    skip_blanks_and_comments(m_state);
    if (not m_state)
        return {};

    const StringView line = m_state.str;
    const char* start = m_state.pos;

    const char c = *m_state.pos;
    if (c == '"' or c == '\'')
    {
        start = ++m_state.pos;
        ParseResult quoted = parse_quoted(m_state, c);
        if (throw_on_unterminated and not quoted.terminated)
            throw parse_error{format("unterminated string {0}...{0}", c)};
        return Token{c == '"' ? Token::Type::RawEval
                              : Token::Type::RawQuoted,
                     start - line.begin(), std::move(quoted.content),
                     quoted.terminated};
    }
    else if (c == '%')
    {
        ++m_state.pos;
        return parse_percent_token(m_state, throw_on_unterminated);
    }
    else if (is_command_separator(c))
        return Token{Token::Type::CommandSeparator,
                     ++m_state.pos - line.begin(), {}};
    else
    {
        if (c == '\\' and m_state.pos + 1 != m_state.str.end())
        {
            const char next = m_state.pos[1];
            if (next == '%' or next == '\'' or next == '"')
                ++m_state.pos;
        }
        return Token{Token::Type::Raw, start - line.begin(), parse_unquoted(m_state)};
    }
    return {};
}

template<typename Postprocess>
String expand_impl(StringView str, const Context& context,
                   const ShellContext& shell_context,
                   Postprocess postprocess)
{
    ParseState state{str, str.begin()};
    String res;
    auto beg = state.pos;
    while (state)
    {
        if (*state.pos++ == '%')
        {
            if (state and *state.pos == '%')
            {
                res += StringView{beg, state.pos};
                beg = ++state.pos;
            }
            else
            {
                res += StringView{beg, state.pos-1};
                String token;
                expand_token(parse_percent_token(state, true), context, shell_context, token);
                res += postprocess(token);
                beg = state.pos;
            }
        }
    }
    res += StringView{beg, state.pos};
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

StringView resolve_alias(const Context& context, StringView name)
{
    auto alias = context.aliases()[name];
    return alias.empty() ? name : alias;
}

void CommandManager::execute_single_command(CommandParameters params,
                                            Context& context,
                                            const ShellContext& shell_context)
{
    if (params.empty())
        return;

    constexpr int max_command_depth = 100;
    if (m_command_depth > max_command_depth)
        throw runtime_error("maximum nested command depth hit");

    ++m_command_depth;
    auto pop_depth = on_scope_end([this] { --m_command_depth; });

    auto command_it = m_commands.find(resolve_alias(context, params[0]));
    if (command_it == m_commands.end())
        throw runtime_error("no such command");

    auto debug_flags = context.options()["debug"].get<DebugFlags>();
    if (debug_flags & DebugFlags::Commands)
        write_to_debug_buffer(format("command {}", join(params, ' ')));

    auto profile = on_scope_end([&, start = (debug_flags & DebugFlags::Profile) ? Clock::now() : Clock::time_point{}] {
        if (not (debug_flags & DebugFlags::Profile))
            return;
        auto full = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
        write_to_debug_buffer(format("command {} took {} us", params[0], full.count()));
    });

    command_it->value.func({{params.begin()+1, params.end()}, command_it->value.param_desc},
                           context, shell_context);
}

void CommandManager::execute(StringView command_line,
                             Context& context, const ShellContext& shell_context)
{
    CommandParser parser(command_line);

    ByteCount command_pos{};
    Vector<String> params;
    while (true)
    {
        Optional<Token> token = parser.read_token(true);
        if (not token or token->type == Token::Type::CommandSeparator)
        {
            try
            {
                execute_single_command(params, context, shell_context);
            }
            catch (failure& error)
            {
                throw;
            }
            catch (runtime_error& error)
            {
                auto coord = compute_coord(command_line.substr(0_byte, command_pos));
                error.set_what(format("{}:{}: '{}': {}", coord.line+1, coord.column+1,
                                      params[0], error.what()));
                throw;
            }

            if (not token)
                return;

            params.clear();
            continue;
        }

        if (params.empty())
            command_pos = token->pos;

        if (token->type == Token::Type::ArgExpand and token->content == '@')
            params.insert(params.end(), shell_context.params.begin(),
                          shell_context.params.end());
        else
            expand_token(*std::move(token), context, shell_context, params);
    }
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

static Completions complete_expansion(const Context& context, CompletionFlags flags,
                                      Token token, ByteCount start,
                                      ByteCount cursor_pos, ByteCount pos_in_token)
{
    switch (token.type) {
    case Token::Type::RegisterExpand:
        return { start, cursor_pos,
                 RegisterManager::instance().complete_register_name(
                     token.content, pos_in_token) };

    case Token::Type::OptionExpand:
        return { start, cursor_pos,
                 GlobalScope::instance().option_registry().complete_option_name(
                     token.content, pos_in_token) };

    case Token::Type::ShellExpand:
        return offset_pos(shell_complete(context, flags, token.content,
                                         pos_in_token), start);

    case Token::Type::ValExpand:
        return { start, cursor_pos,
                 ShellManager::instance().complete_env_var(
                     token.content, pos_in_token) };

    case Token::Type::FileExpand:
    {
        const auto& ignored_files = context.options()["ignored_files"].get<Regex>();
        return { start, cursor_pos, complete_filename(
                 token.content, ignored_files, pos_in_token, FilenameFlags::Expand) };
    }

    default:
        kak_assert(false);
        throw runtime_error("unknown expansion");
    }
}

static Completions complete_raw_eval(const Context& context, CompletionFlags flags,
                                     StringView prefix, ByteCount start,
                                     ByteCount cursor_pos, ByteCount pos_in_token)
{
    ParseState state{prefix, prefix.begin()};
    while (state)
    {
        if (*state.pos++ == '%')
        {
            if (state and *state.pos == '%')
                ++state.pos;
            else
            {
                auto token = parse_percent_token(state, false);
                if (token.terminated)
                    continue;
                if (token.type == Token::Type::Raw or token.type == Token::Type::RawQuoted)
                    return {};
                return complete_expansion(context, flags, token,
                                          start + token.pos, cursor_pos,
                                          pos_in_token - token.pos);
            }
        }
    }
    return {};
}

Completions CommandManager::complete(const Context& context,
                                     CompletionFlags flags,
                                     StringView command_line,
                                     ByteCount cursor_pos)
{
    auto prefix = command_line.substr(0_byte, cursor_pos);
    CommandParser parser{prefix};
    const char* cursor = prefix.begin() + (int)cursor_pos;
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
        tokens.push_back({Token::Type::Raw, prefix.length(), {}});
    kak_assert(not tokens.empty());
    const auto& token = tokens.back();

    if (token.terminated) // do not complete past explicit token close
        return Completions{};

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
    const ByteCount pos_in_token = cursor_pos - start;

    // command name completion
    if (tokens.size() == 1 and (token.type == Token::Type::Raw or
                                token.type == Token::Type::RawQuoted))
    {
        return offset_pos(requote(complete_command_name(context, prefix), token.type), start);
    }

    switch (token.type)
    {
    case Token::Type::RegisterExpand:
    case Token::Type::OptionExpand:
    case Token::Type::ShellExpand:
    case Token::Type::ValExpand:
    case Token::Type::FileExpand:
        return complete_expansion(context, flags, token, start, cursor_pos, pos_in_token);

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

        auto& command = command_it->value;

        const bool has_switches = not command.param_desc.switches.empty();
        auto is_switch = [=](StringView s) { return has_switches and s.substr(0_byte, 1_byte) == "-"; };

        if (is_switch(token.content)
            and not contains(tokens | drop(1) | transform(&Token::content), "--"))
        {
            auto switches = Kakoune::complete(token.content.substr(1_byte), pos_in_token,
                                              concatenated(command.param_desc.switches
                                                               | transform(&SwitchMap::Item::key),
                                                           ConstArrayView<String>{"-"}));
            return switches.empty()
                    ? Completions{}
                    : Completions{start+1, cursor_pos, std::move(switches), Completions::Flags::Menu};
        }
        if (not command.completer)
            return Completions{};

        auto params = tokens | skip(1) | transform(&Token::content) | filter(std::not_fn(is_switch)) | gather<Vector>();
        auto index = params.size() - 1;

        return offset_pos(requote(command.completer(context, flags, params, index, pos_in_token), token.type), start);
    }
    case Token::Type::RawEval:
        return complete_raw_eval(context, flags, token.content, start, cursor_pos, pos_in_token);
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
        auto check_quoted_impl = [&](auto type_hint) {
            ParseState state{str, str.begin()};
            const decltype(type_hint) delimiter = *state.pos++;
            auto quoted = parse_quoted(state, delimiter);
            kak_assert(quoted.terminated == terminated);
            kak_assert(quoted.content == content);
        };
        check_quoted_impl(Codepoint{});
        check_quoted_impl(char{});
    };
    check_quoted("'abc'", true, "abc");
    check_quoted("'abc''def", false, "abc'def");
    check_quoted("'abc''def'''", true, "abc'def'");
    check_quoted(StringView("'abc''def'", 5), true, "abc");

    auto check_balanced = [](StringView str, bool terminated, StringView content)
    {
        ParseState state{str, str.begin()+1};
        auto quoted = parse_quoted_balanced<'{', '}'>(state);
        kak_assert(quoted.terminated == terminated);
        kak_assert(quoted.content == content);
    };
    check_balanced("{abc}", true, "abc");
    check_balanced("{abc{def}}", true, "abc{def}");
    check_balanced("{{abc}{def}", false, "{abc}{def}");

    auto check_unquoted = [](StringView str, StringView content)
    {
        ParseState state{str, str.begin()};
        kak_assert(parse_unquoted(state) == content);
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
