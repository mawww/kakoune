#include "commands.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "completion.hh"
#include "context.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "hash_map.hh"
#include "highlighter.hh"
#include "highlighters.hh"
#include "input_handler.hh"
#include "insert_completer.hh"
#include "normal.hh"
#include "option_manager.hh"
#include "option_types.hh"
#include "parameters_parser.hh"
#include "ranges.hh"
#include "ranked_match.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "user_interface.hh"
#include "window.hh"

#include <functional>
#include <utility>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__GLIBC__) || defined(__CYGWIN__)
#include <malloc.h>
#endif

namespace Kakoune
{

extern const char* version;

namespace
{

Buffer* open_fifo(StringView name, StringView filename, Buffer::Flags flags, bool scroll)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0)
       throw runtime_error(format("unable to open '{}'", filename));

    return create_fifo_buffer(name.str(), fd, flags, scroll);
}

template<typename... Completers> struct PerArgumentCommandCompleter;

template<> struct PerArgumentCommandCompleter<>
{
    Completions operator()(const Context&, CompletionFlags, CommandParameters,
                           size_t, ByteCount) const { return {}; }
};

template<typename Completer, typename... Rest>
struct PerArgumentCommandCompleter<Completer, Rest...> : PerArgumentCommandCompleter<Rest...>
{
    template<typename C, typename... R>
        requires (not std::is_base_of_v<PerArgumentCommandCompleter<>, std::remove_reference_t<C>>)
    PerArgumentCommandCompleter(C&& completer, R&&... rest)
      : PerArgumentCommandCompleter<Rest...>(std::forward<R>(rest)...),
        m_completer(std::forward<C>(completer)) {}

    Completions operator()(const Context& context, CompletionFlags flags,
                           CommandParameters params, size_t token_to_complete,
                           ByteCount pos_in_token) const
    {
        if (token_to_complete == 0)
        {
            const String& arg = token_to_complete < params.size() ?
                                params[token_to_complete] : String();
            return m_completer(context, flags, arg, pos_in_token);
        }
        return PerArgumentCommandCompleter<Rest...>::operator()(
            context, flags, params.subrange(1),
            token_to_complete-1, pos_in_token);
    }

    Completer m_completer;
};

template<typename... Completers>
PerArgumentCommandCompleter<std::decay_t<Completers>...>
make_completer(Completers&&... completers)
{
    return {std::forward<Completers>(completers)...};
}

template<typename Completer>
auto add_flags(Completer completer, Completions::Flags completions_flags)
{
    return [completer=std::move(completer), completions_flags]
           (const Context& context, CompletionFlags flags, StringView prefix, ByteCount cursor_pos) {
        Completions res = completer(context, flags, prefix, cursor_pos);
        res.flags |= completions_flags;
        return res;
    };
}

template<typename Completer>
auto menu(Completer completer)
{
    return add_flags(std::move(completer), Completions::Flags::Menu);
}

template<bool menu>
auto filename_completer = make_completer(
    [](const Context& context, CompletionFlags flags, StringView prefix, ByteCount cursor_pos)
    { return Completions{ 0_byte, cursor_pos,
                          complete_filename(prefix,
                                            context.options()["ignored_files"].get<Regex>(),
                                            cursor_pos, FilenameFlags::Expand),
                                            menu ? Completions::Flags::Menu : Completions::Flags::None}; });

template<bool ignore_current = false>
static Completions complete_buffer_name(const Context& context, CompletionFlags flags,
                                        StringView prefix, ByteCount cursor_pos)
{
    struct RankedMatchAndBuffer : RankedMatch
    {
        RankedMatchAndBuffer(RankedMatch  m, const Buffer* b)
            : RankedMatch{std::move(m)}, buffer{b} {}

        using RankedMatch::operator==;
        using RankedMatch::operator<;

        const Buffer* buffer;
    };

    StringView query = prefix.substr(0, cursor_pos);
    Vector<RankedMatchAndBuffer> filename_matches;
    Vector<RankedMatchAndBuffer> matches;
    for (const auto& buffer : BufferManager::instance())
    {
        if (ignore_current and buffer.get() == &context.buffer())
            continue;

        StringView bufname = buffer->display_name();
        if (buffer->flags() & Buffer::Flags::File)
        {
            if (RankedMatch match{split_path(bufname).second, query})
            {
                filename_matches.emplace_back(match, buffer.get());
                continue;
            }
        }
        if (RankedMatch match{bufname, query})
            matches.emplace_back(match, buffer.get());
    }
    std::sort(filename_matches.begin(), filename_matches.end());
    std::sort(matches.begin(), matches.end());

    CandidateList res;
    for (auto& match : filename_matches)
        res.push_back(match.buffer->display_name());
    for (auto& match : matches)
        res.push_back(match.buffer->display_name());

    return { 0, cursor_pos, res };
}

template<typename Func>
auto make_single_word_completer(Func&& func)
{
    return make_completer(
        [func = std::move(func)](const Context& context, CompletionFlags flags,
               StringView prefix, ByteCount cursor_pos) -> Completions {
            auto candidate = { func(context) };
            return { 0_byte, cursor_pos, complete(prefix, cursor_pos, candidate) }; });
}

const ParameterDesc no_params{ {}, ParameterDesc::Flags::None, 0, 0 };
const ParameterDesc single_param{ {}, ParameterDesc::Flags::None, 1, 1 };
const ParameterDesc single_optional_param{ {}, ParameterDesc::Flags::None, 0, 1 };
const ParameterDesc double_params{ {}, ParameterDesc::Flags::None, 2, 2 };

static Completions complete_scope(const Context&, CompletionFlags,
                                  StringView prefix, ByteCount cursor_pos)
{
   static constexpr StringView scopes[] = { "global", "buffer", "window", };
   return { 0_byte, cursor_pos, complete(prefix, cursor_pos, scopes) };
}

static Completions complete_scope_including_current(const Context&, CompletionFlags,
                                  StringView prefix, ByteCount cursor_pos)
{
   static constexpr StringView scopes[] = { "global", "buffer", "window", "current" };
   return { 0_byte, cursor_pos, complete(prefix, cursor_pos, scopes) };
}

static Completions complete_scope_no_global(const Context&, CompletionFlags,
                                            StringView prefix, ByteCount cursor_pos)
{
   static constexpr StringView scopes[] = { "buffer", "window", "current" };
   return { 0_byte, cursor_pos, complete(prefix, cursor_pos, scopes) };
}


static Completions complete_command_name(const Context& context, CompletionFlags,
                                         StringView prefix, ByteCount cursor_pos)
{
   return CommandManager::instance().complete_command_name(
       context, prefix.substr(0, cursor_pos));
}

struct ShellScriptCompleter
{
    ShellScriptCompleter(String shell_script,
                         Completions::Flags flags = Completions::Flags::None)
      : m_shell_script{std::move(shell_script)}, m_flags(flags) {}

    Completions operator()(const Context& context, CompletionFlags flags,
                           CommandParameters params, size_t token_to_complete,
                           ByteCount pos_in_token)
    {
        if (flags & CompletionFlags::Fast) // no shell on fast completion
            return Completions{};

        ShellContext shell_context{
            params,
            { { "token_to_complete", to_string(token_to_complete) },
              { "pos_in_token",      to_string(pos_in_token) } }
        };
        String output = ShellManager::instance().eval(m_shell_script, context, {},
                                                      ShellManager::Flags::WaitForStdout,
                                                      shell_context).first;
        CandidateList candidates;
        for (auto&& candidate : output | split<StringView>('\n'))
            candidates.push_back(candidate.str());

        return {0_byte, pos_in_token, std::move(candidates), m_flags};
    }
private:
    String m_shell_script;
    Completions::Flags m_flags;
};

struct ShellCandidatesCompleter
{
    ShellCandidatesCompleter(String shell_script,
                             Completions::Flags flags = Completions::Flags::None)
      : m_shell_script{std::move(shell_script)}, m_flags(flags) {}

    Completions operator()(const Context& context, CompletionFlags flags,
                           CommandParameters params, size_t token_to_complete,
                           ByteCount pos_in_token)
    {
        if (flags & CompletionFlags::Start)
            m_token = -1;

        if (m_token != token_to_complete)
        {
            ShellContext shell_context{
                params,
                { { "token_to_complete", to_string(token_to_complete) } }
            };
            String output = ShellManager::instance().eval(m_shell_script, context, {},
                                                          ShellManager::Flags::WaitForStdout,
                                                          shell_context).first;
            m_candidates.clear();
            for (auto c : output | split<StringView>('\n'))
                m_candidates.emplace_back(c.str(), used_letters(c));
            m_token = token_to_complete;
        }

        StringView query = params[token_to_complete].substr(0, pos_in_token);
        UsedLetters query_letters = used_letters(query);
        Vector<RankedMatch> matches;
        for (const auto& candidate : m_candidates)
        {
            if (RankedMatch match{candidate.first, candidate.second, query, query_letters})
                matches.push_back(match);
        }

        constexpr size_t max_count = 100;
        CandidateList res;
        // Gather best max_count matches
        for_n_best(matches, max_count, [](auto& lhs, auto& rhs) { return rhs < lhs; },
                   [&] (const RankedMatch& m) {
            if (not res.empty() and res.back() == m.candidate())
                return false;
            res.push_back(m.candidate().str());
            return true;
        });

        return Completions{0_byte, pos_in_token, std::move(res), m_flags};
    }

private:
    String m_shell_script;
    Vector<std::pair<String, UsedLetters>, MemoryDomain::Completion> m_candidates;
    int m_token = -1;
    Completions::Flags m_flags;
};

template<typename Completer>
struct PromptCompleterAdapter
{
    PromptCompleterAdapter(Completer completer) : m_completer{std::move(completer)} {}

    operator PromptCompleter() &&
    {
        if (not m_completer)
            return {};
        return [completer=std::move(m_completer)](const Context& context, CompletionFlags flags,
                                                  StringView prefix, ByteCount cursor_pos) {
            return completer(context, flags, {String{String::NoCopy{}, prefix}}, 0, cursor_pos);
        };
    }

private:
    Completer m_completer;
};

Scope* get_scope_ifp(StringView scope, const Context& context)
{
    if (prefix_match("global", scope))
        return &GlobalScope::instance();
    else if (prefix_match("buffer", scope))
        return &context.buffer();
    else if (prefix_match("window", scope))
        return &context.window();
    else if (prefix_match(scope, "buffer="))
        return &BufferManager::instance().get_buffer(scope.substr(7_byte));
    return nullptr;
}

Scope& get_scope(StringView scope, const Context& context)
{
    if (auto s = get_scope_ifp(scope, context))
        return *s;
    throw runtime_error(format("no such scope: '{}'", scope));
}

struct CommandDesc
{
    const char* name;
    const char* alias;
    const char* docstring;
    ParameterDesc params;
    CommandFlags flags;
    CommandHelper helper;
    CommandCompleter completer;
    void (*func)(const ParametersParser&, Context&, const ShellContext&);
};

template<bool force_reload>
void edit(const ParametersParser& parser, Context& context, const ShellContext&)
{
    const bool scratch = (bool)parser.get_switch("scratch");

    if (parser.positional_count() == 0 and not force_reload and not scratch)
        throw wrong_argument_count();

    const bool no_hooks = context.hooks_disabled();
    const auto flags = (no_hooks ? Buffer::Flags::NoHooks : Buffer::Flags::None) |
       (parser.get_switch("debug") ? Buffer::Flags::Debug : Buffer::Flags::None);

    auto& buffer_manager = BufferManager::instance();
    const auto& name = parser.positional_count() > 0 ?
        parser[0] : (scratch ? generate_buffer_name("*scratch-{}*") : context.buffer().name());

    Buffer* buffer = buffer_manager.get_buffer_ifp(name);
    if (scratch)
    {
        if (parser.get_switch("readonly") or parser.get_switch("fifo") or parser.get_switch("scroll"))
            throw runtime_error("scratch is not compatible with readonly, fifo or scroll");

        if (buffer == nullptr or force_reload)
        {
            if (buffer != nullptr and force_reload)
                buffer_manager.delete_buffer(*buffer);
            buffer = create_buffer_from_string(std::move(name), flags, {});
        }
        else if (buffer->flags() & Buffer::Flags::File)
            throw runtime_error(format("buffer '{}' exists but is not a scratch buffer", name));
    }
    else if (force_reload and buffer and buffer->flags() & Buffer::Flags::File)
    {
        reload_file_buffer(*buffer);
    }
    else
    {
        if (auto fifo = parser.get_switch("fifo"))
            buffer = open_fifo(name, *fifo, flags, (bool)parser.get_switch("scroll"));
        else if (not buffer)
        {
            buffer = parser.get_switch("existing") ? open_file_buffer(name, flags)
                                                   : open_or_create_file_buffer(name, flags);
            if (buffer->flags() & Buffer::Flags::New)
                context.print_status({ format("new file '{}'", name),
                                       context.faces()["StatusLine"] });
        }

        buffer->flags() &= ~Buffer::Flags::NoHooks;
        if (parser.get_switch("readonly"))
        {
            buffer->flags() |= Buffer::Flags::ReadOnly;
            buffer->options().get_local_option("readonly").set(true);
        }
    }

    Buffer* current_buffer = context.has_buffer() ? &context.buffer() : nullptr;

    const size_t param_count = parser.positional_count();
    if (current_buffer and (buffer != current_buffer or param_count > 1))
        context.push_jump();

    if (buffer != current_buffer)
        context.change_buffer(*buffer);
    buffer = &context.buffer(); // change_buffer hooks might change the buffer again

    if (parser.get_switch("fifo") and not parser.get_switch("scroll"))
        context.selections_write_only() = { *buffer, Selection{} };
    else if (param_count > 1 and not parser[1].empty())
    {
        int line = std::max(0, str_to_int(parser[1]) - 1);
        int column = param_count > 2 and not parser[2].empty() ?
                     std::max(0, str_to_int(parser[2]) - 1) : 0;

        auto& buffer = context.buffer();
        context.selections_write_only() = { buffer, buffer.clamp({ line,  column }) };
        if (context.has_window())
            context.window().center_line(context.selections().main().cursor().line);
    }
}

ParameterDesc edit_params{
    { { "existing", { false, "fail if the file does not exist, do not open a new file" } },
      { "scratch",  { false, "create a scratch buffer, not linked to a file" } },
      { "debug",    { false, "create buffer as debug output" } },
      { "fifo",     { true,  "create a buffer reading its content from a named fifo" } },
      { "readonly", { false, "create a buffer in readonly mode" } },
      { "scroll",   { false, "place the initial cursor so that the fifo will scroll to show new data" } } },
      ParameterDesc::Flags::None, 0, 3
};
const CommandDesc edit_cmd = {
    "edit",
    "e",
    "edit [<switches>] <filename> [<line> [<column>]]: open the given filename in a buffer",
    edit_params,
    CommandFlags::None,
    CommandHelper{},
    filename_completer<false>,
    edit<false>
};

const CommandDesc force_edit_cmd = {
    "edit!",
    "e!",
    "edit! [<switches>] <filename> [<line> [<column>]]: open the given filename in a buffer, "
    "force reload if needed",
    edit_params,
    CommandFlags::None,
    CommandHelper{},
    filename_completer<false>,
    edit<true>
};

const ParameterDesc write_params = {
    {
        { "sync", { false, "force the synchronization of the file onto the filesystem" } },
        { "method", { true, "explicit writemethod (replace|overwrite)" } },
        { "force", { false, "Allow overwriting existing file with explicit filename" } }
    },
    ParameterDesc::Flags::None, 0, 1
};

const ParameterDesc write_params_except_force = {
    {
        { "sync", { false, "force the synchronization of the file onto the filesystem" } },
        { "method", { true, "explicit writemethod (replace|overwrite)" } },
    },
    ParameterDesc::Flags::None, 0, 1
};

auto parse_write_method(StringView str)
{
    constexpr auto desc = enum_desc(Meta::Type<WriteMethod>{});
    auto it = find_if(desc, [str](const EnumDesc<WriteMethod>& d) { return d.name == str; });
    if (it == desc.end())
        throw runtime_error(format("invalid writemethod '{}'", str));
    return it->value;
}

void do_write_buffer(Context& context, Optional<String> filename, WriteFlags flags, Optional<WriteMethod> write_method = {})
{
    Buffer& buffer = context.buffer();
    const bool is_file = (bool)(buffer.flags() & Buffer::Flags::File);

    if (not filename and !is_file)
        throw runtime_error("cannot write a non file buffer without a filename");

    const bool is_readonly = (bool)(context.buffer().flags() & Buffer::Flags::ReadOnly);
    // if the buffer is in read-only mode and we try to save it directly
    // or we try to write to it indirectly using e.g. a symlink, throw an error
    if (is_file and is_readonly and
        (not filename or real_path(*filename) == buffer.name()))
        throw runtime_error("cannot overwrite the buffer when in readonly mode");

    auto effective_filename = filename ? parse_filename(*filename) : buffer.name();
    if (filename and not (flags & WriteFlags::Force) and
        real_path(effective_filename) != buffer.name() and
        regular_file_exists(effective_filename))
        throw runtime_error("cannot overwrite existing file without -force");

    auto method = write_method.value_or_compute([&] { return context.options()["writemethod"].get<WriteMethod>(); });

    context.hooks().run_hook(Hook::BufWritePre, effective_filename, context);
    write_buffer_to_file(buffer, effective_filename, method, flags);
    context.hooks().run_hook(Hook::BufWritePost, effective_filename, context);
}

template<bool force = false>
void write_buffer(const ParametersParser& parser, Context& context, const ShellContext&)
{
    return do_write_buffer(context,
                           parser.positional_count() > 0 ? parser[0] : Optional<String>{},
                           (parser.get_switch("sync") ? WriteFlags::Sync : WriteFlags::None) |
                           (parser.get_switch("force") or force ? WriteFlags::Force : WriteFlags::None),
                           parser.get_switch("method").map(parse_write_method));
}

const CommandDesc write_cmd = {
    "write",
    "w",
    "write [<switches>] [<filename>]: write the current buffer to its file "
    "or to <filename> if specified",
    write_params,
    CommandFlags::None,
    CommandHelper{},
    filename_completer<false>,
    write_buffer,
};

const CommandDesc force_write_cmd = {
    "write!",
    "w!",
    "write! [<switches>] [<filename>]: write the current buffer to its file "
    "or to <filename> if specified, even when the file is write protected",
    write_params_except_force,
    CommandFlags::None,
    CommandHelper{},
    filename_completer<false>,
    write_buffer<true>,
};

void write_all_buffers(const Context& context, bool sync = false, Optional<WriteMethod> write_method = {})
{
    // Copy buffer list because hooks might be creating/deleting buffers
    Vector<SafePtr<Buffer>> buffers;
    for (auto& buffer : BufferManager::instance())
        buffers.emplace_back(buffer.get());

    for (auto& buffer : buffers)
    {
        if ((buffer->flags() & Buffer::Flags::File) and
            ((buffer->flags() & Buffer::Flags::New) or
             buffer->is_modified())
            and !(buffer->flags() & Buffer::Flags::ReadOnly))
        {
            auto method = write_method.value_or_compute([&] { return context.options()["writemethod"].get<WriteMethod>(); });
            auto flags = sync ? WriteFlags::Sync : WriteFlags::None;
            buffer->run_hook_in_own_context(Hook::BufWritePre, buffer->name(), context.name());
            write_buffer_to_file(*buffer, buffer->name(), method, flags);
            buffer->run_hook_in_own_context(Hook::BufWritePost, buffer->name(), context.name());
        }
    }
}

const CommandDesc write_all_cmd = {
    "write-all",
    "wa",
    "write-all [<switches>]: write all changed buffers that are associated to a file",
    ParameterDesc{
        write_params_except_force.switches,
        ParameterDesc::Flags::None, 0, 0
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&){
        write_all_buffers(context,
                          (bool)parser.get_switch("sync"),
                          parser.get_switch("method").map(parse_write_method));
    }
};

static void ensure_all_buffers_are_saved()
{
    auto is_modified = [](const std::unique_ptr<Buffer>& buf) {
        return (buf->flags() & Buffer::Flags::File) and buf->is_modified();
    };

    auto it = find_if(BufferManager::instance(), is_modified);
    const auto end = BufferManager::instance().end();
    if (it == end)
        return;

    String message = format("{} modified buffers remaining: [",
                            std::count_if(it, end, is_modified));
    while (it != end)
    {
        message += (*it)->name();
        it = std::find_if(it+1, end, is_modified);
        message += (it != end) ? ", " : "]";
    }
    throw runtime_error(message);
}

template<bool force>
void kill(const ParametersParser& parser, Context& context, const ShellContext&)
{
    auto& client_manager = ClientManager::instance();

    if (not force)
        ensure_all_buffers_are_saved();

    const int status = parser.positional_count() > 0 ? str_to_int(parser[0]) : 0;
    while (not client_manager.empty())
        client_manager.remove_client(**client_manager.begin(), true, status);

    throw kill_session{status};
}

const CommandDesc kill_cmd = {
    "kill",
    nullptr,
    "kill [<exit status>]: terminate the current session, the server and all clients connected. "
    "An optional integer parameter can set the server and client processes exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    kill<false>
};


const CommandDesc force_kill_cmd = {
    "kill!",
    nullptr,
    "kill! [<exit status>]: force the termination of the current session, the server and all clients connected. "
    "An optional integer parameter can set the server and client processes exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    kill<true>
};

template<bool force>
void quit(const ParametersParser& parser, Context& context, const ShellContext&)
{
    if (not force and ClientManager::instance().count() == 1 and not Server::instance().is_daemon())
        ensure_all_buffers_are_saved();

    const int status = parser.positional_count() > 0 ? str_to_int(parser[0]) : 0;
    ClientManager::instance().remove_client(context.client(), true, status);
}

const CommandDesc quit_cmd = {
    "quit",
    "q",
    "quit [<exit status>]: quit current client, and the kakoune session if the client is the last "
    "(if not running in daemon mode). "
    "An optional integer parameter can set the client exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    quit<false>
};

const CommandDesc force_quit_cmd = {
    "quit!",
    "q!",
    "quit! [<exit status>]: quit current client, and the kakoune session if the client is the last "
    "(if not running in daemon mode). Force quit even if the client is the "
    "last and some buffers are not saved. "
    "An optional integer parameter can set the client exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    quit<true>
};

template<bool force>
void write_quit(const ParametersParser& parser, Context& context,
                const ShellContext& shell_context)
{
    do_write_buffer(context, {},
                    parser.get_switch("sync") ? WriteFlags::Sync : WriteFlags::None,
                    parser.get_switch("method").map(parse_write_method));
    quit<force>(parser, context, shell_context);
}

const CommandDesc write_quit_cmd = {
    "write-quit",
    "wq",
    "write-quit [<switches>] [<exit status>]: write current buffer and quit current client. "
    "An optional integer parameter can set the client exit status",
    write_params_except_force,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    write_quit<false>
};

const CommandDesc force_write_quit_cmd = {
    "write-quit!",
    "wq!",
    "write-quit! [<switches>] [<exit status>] write: current buffer and quit current client, even if other buffers are not saved. "
    "An optional integer parameter can set the client exit status",
    write_params_except_force,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    write_quit<true>
};

const CommandDesc write_all_quit_cmd = {
    "write-all-quit",
    "waq",
    "write-all-quit [<switches>] [<exit status>]: write all buffers associated to a file and quit current client. "
    "An optional integer parameter can set the client exit status.",
    write_params_except_force,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        write_all_buffers(context,
                          (bool)parser.get_switch("sync"),
                          parser.get_switch("method").map(parse_write_method));
        quit<false>(parser, context, shell_context);
    }
};

const CommandDesc buffer_cmd = {
    "buffer",
    "b",
    "buffer <name>: set buffer to edit in current client",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(complete_buffer_name<true>)),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        Buffer& buffer = BufferManager::instance().get_buffer(parser[0]);
        if (&buffer != &context.buffer())
        {
            context.push_jump();
            context.change_buffer(buffer);
        }
    }
};

template<bool next>
void cycle_buffer(const ParametersParser& parser, Context& context, const ShellContext&)
{
    Buffer* oldbuf = &context.buffer();
    auto it = find_if(BufferManager::instance(),
                      [oldbuf](const std::unique_ptr<Buffer>& lhs)
                      { return lhs.get() == oldbuf; });
    kak_assert(it != BufferManager::instance().end());

    Buffer* newbuf = nullptr;
    auto cycle = [&] {
        if (not next)
        {
            if (it == BufferManager::instance().begin())
                it = BufferManager::instance().end();
            --it;
        }
        else
        {
            if (++it == BufferManager::instance().end())
                it = BufferManager::instance().begin();
        }
        newbuf = it->get();
    };
    cycle();
    while (newbuf != oldbuf and newbuf->flags() & Buffer::Flags::Debug)
        cycle();

    if (newbuf != oldbuf)
    {
        context.push_jump();
        context.change_buffer(*newbuf);
    }
}

const CommandDesc buffer_next_cmd = {
    "buffer-next",
    "bn",
    "buffer-next: move to the next buffer in the list",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    cycle_buffer<true>
};

const CommandDesc buffer_previous_cmd = {
    "buffer-previous",
    "bp",
    "buffer-previous: move to the previous buffer in the list",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    cycle_buffer<false>
};

template<bool force>
void delete_buffer(const ParametersParser& parser, Context& context, const ShellContext&)
{
    BufferManager& manager = BufferManager::instance();
    Buffer& buffer = parser.positional_count() == 0 ? context.buffer() : manager.get_buffer(parser[0]);
    if (not force and (buffer.flags() & Buffer::Flags::File) and buffer.is_modified())
        throw runtime_error(format("buffer '{}' is modified", buffer.name()));

    manager.delete_buffer(buffer);
    context.forget_buffer(buffer);
}

const CommandDesc delete_buffer_cmd = {
    "delete-buffer",
    "db",
    "delete-buffer [name]: delete current buffer or the buffer named <name> if given",
    single_optional_param,
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(complete_buffer_name<false>)),
    delete_buffer<false>
};

const CommandDesc force_delete_buffer_cmd = {
    "delete-buffer!",
    "db!",
    "delete-buffer! [name]: delete current buffer or the buffer named <name> if "
    "given, even if the buffer is unsaved",
    single_optional_param,
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(complete_buffer_name<false>)),
    delete_buffer<true>
};

const CommandDesc rename_buffer_cmd = {
    "rename-buffer",
    nullptr,
    "rename-buffer <name>: change current buffer name",
    ParameterDesc{
        {
            { "scratch",  { false, "convert a file buffer to a scratch buffer" } },
            { "file",  { false, "convert a scratch buffer to a file buffer" } }
        },
        ParameterDesc::Flags::None, 1, 1
    },
    CommandFlags::None,
    CommandHelper{},
    filename_completer<false>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (parser.get_switch("scratch") and parser.get_switch("file"))
            throw runtime_error("scratch and file are incompatible switches");

        auto& buffer = context.buffer();
        if (parser.get_switch("scratch"))
            buffer.flags() &= ~(Buffer::Flags::File | Buffer::Flags::New);
        if (parser.get_switch("file"))
            buffer.flags() |= Buffer::Flags::File;

        const bool is_file = (buffer.flags() & Buffer::Flags::File);

        if (not buffer.set_name(is_file ? parse_filename(parser[0]) : parser[0]))
            throw runtime_error(format("unable to change buffer name to '{}': a buffer with this name already exists", parser[0]));
    }
};

static constexpr auto highlighter_scopes = { "global/", "buffer/", "window/", "shared/" };

template<bool add>
Completions highlighter_cmd_completer(
    const Context& context, CompletionFlags flags, CommandParameters params,
    size_t token_to_complete, ByteCount pos_in_token)
{
    if (token_to_complete == 0)
    {

        StringView path = params[0];
        auto sep_it = find(path, '/');
        if (sep_it == path.end())
           return { 0_byte, pos_in_token, complete(path, pos_in_token, highlighter_scopes),
                    Completions::Flags::Menu };

        StringView scope{path.begin(), sep_it};
        HighlighterGroup* root = nullptr;
        if (scope == "shared")
            root = &SharedHighlighters::instance();
        else if (auto* s = get_scope_ifp(scope, context))
            root = &s->highlighters().group();
        else
            return {};

        auto offset = scope.length() + 1;
        return offset_pos(root->complete_child(StringView{sep_it+1, path.end()}, pos_in_token - offset, add), offset);
    }
    else if (add and token_to_complete == 1)
    {
        StringView name = params[1];
        return { 0_byte, name.length(), complete(name, pos_in_token, HighlighterRegistry::instance() | transform(&HighlighterRegistry::Item::key)),
                 Completions::Flags::Menu };
    }
    else
        return {};
}

Highlighter& get_highlighter(const Context& context, StringView path)
{
    if (not path.empty() and path.back() == '/')
        path = path.substr(0_byte, path.length() - 1);

    auto sep_it = find(path, '/');
    StringView scope{path.begin(), sep_it};
    auto* root = (scope == "shared") ? static_cast<HighlighterGroup*>(&SharedHighlighters::instance())
                                     : static_cast<HighlighterGroup*>(&get_scope(scope, context).highlighters().group());
    if (sep_it != path.end())
        return root->get_child(StringView{sep_it+1, path.end()});
    return *root;
}

static void redraw_relevant_clients(Context& context, StringView highlighter_path)
{
    StringView scope{highlighter_path.begin(), find(highlighter_path, '/')};
    if (scope == "window")
        context.window().force_redraw();
    else if (scope == "buffer" or prefix_match(scope, "buffer="))
    {
        auto& buffer = scope == "buffer" ? context.buffer() : BufferManager::instance().get_buffer(scope.substr(7_byte));
        for (auto&& client : ClientManager::instance())
        {
            if (&client->context().buffer() == &buffer)
                client->context().window().force_redraw();
        }
    }
    else
    {
        for (auto&& client : ClientManager::instance())
            client->context().window().force_redraw();
    }
}

const CommandDesc arrange_buffers_cmd = {
    "arrange-buffers",
    nullptr,
    "arrange-buffers <buffer>...: reorder the buffers in the buffers list\n"
    "    the named buffers will be moved to the front of the buffer list, in the order given\n"
    "    buffers that do not appear in the parameters will remain at the end of the list, keeping their current order",
    ParameterDesc{{}, ParameterDesc::Flags::None, 1},
    CommandFlags::None,
    CommandHelper{},
    [](const Context& context, CompletionFlags flags, CommandParameters params, size_t, ByteCount cursor_pos)
    {
        return menu(complete_buffer_name<false>)(context, flags, params.back(), cursor_pos);
    },
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        BufferManager::instance().arrange_buffers(parser.positionals_from(0));
    }
};

const CommandDesc add_highlighter_cmd = {
    "add-highlighter",
    "addhl",
    "add-highlighter [-override] <path>/<name> <type> <type params>...: add a highlighter to the group identified by <path>\n"
    "    <path> is a '/' delimited path or the parent highlighter, starting with either\n"
    "   'global', 'buffer', 'window' or 'shared', if <name> is empty, it will be autogenerated",
    ParameterDesc{
        { { "override", { false, "replace existing highlighter with same path if it exists" } }, },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 2
    },
    CommandFlags::None,
    [](const Context& context, CommandParameters params) -> String
    {
        if (params.size() > 1)
        {
            HighlighterRegistry& registry = HighlighterRegistry::instance();
            auto it = registry.find(params[1]);
            if (it != registry.end())
            {
                auto docstring = it->value.description->docstring;
                auto desc_params = generate_switches_doc(it->value.description->params.switches);

                if (desc_params.empty())
                    return format("{}:\n{}", params[1], indent(docstring));
                else
                {
                    auto desc_indent = Vector<String>{docstring, "Switches:", indent(desc_params)}
                                           | transform([](auto& s) { return indent(s); });
                    return format("{}:\n{}", params[1], join(desc_indent, "\n"));
                }
            }
        }
        return "";
    },
    highlighter_cmd_completer<true>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        auto begin = parser.begin();
        StringView path = *begin++;
        StringView type = *begin++;
        Vector<String> highlighter_params;
        for (; begin != parser.end(); ++begin)
            highlighter_params.push_back(*begin);

        auto it = registry.find(type);
        if (it == registry.end())
            throw runtime_error(format("no such highlighter type: '{}'", type));

        auto slash = find(path | reverse(), '/');
        if (slash == path.rend())
            throw runtime_error("no parent in path");

        auto auto_name = [](ConstArrayView<String> params) {
            return join(params | transform([](StringView s) { return replace(s, "/", "<slash>"); }), "_");
        };

        String name{slash.base(), path.end()};
        Highlighter& parent = get_highlighter(context, {path.begin(), slash.base() - 1});
        parent.add_child(name.empty() ? auto_name(parser.positionals_from(1)) : std::move(name),
                         it->value.factory(highlighter_params, &parent), (bool)parser.get_switch("override"));

        redraw_relevant_clients(context, path);
    }
};

const CommandDesc remove_highlighter_cmd = {
    "remove-highlighter",
    "rmhl",
    "remove-highlighter <path>: remove highlighter identified by <path>",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    highlighter_cmd_completer<false>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        StringView path = parser[0];
        if (not path.empty() and path.back() == '/') // ignore trailing /
            path = path.substr(0_byte, path.length() - 1_byte);

        auto rev_path = path | reverse();
        auto sep_it = find(rev_path, '/');
        if (sep_it == rev_path.end())
            return;
        get_highlighter(context, {path.begin(), sep_it.base()}).remove_child({sep_it.base(), path.end()});
        redraw_relevant_clients(context, path);
    }
};

static Completions complete_hooks(const Context&, CompletionFlags,
                                  StringView prefix, ByteCount cursor_pos)
{
    return { 0_byte, cursor_pos, complete(prefix, cursor_pos, enum_desc(Meta::Type<Hook>{}) | transform(&EnumDesc<Hook>::name)) };
}

const CommandDesc add_hook_cmd = {
    "hook",
    nullptr,
    "hook [<switches>] <scope> <hook_name> <filter> <command>: add <command> in <scope> "
    "to be executed on hook <hook_name> when its parameter matches the <filter> regex\n"
    "<scope> can be:\n"
    "  * global: hook is executed for any buffer or window\n"
    "  * buffer: hook is executed only for the current buffer\n"
    "            (and any window for that buffer)\n"
    "  * window: hook is executed only for the current window\n",
    ParameterDesc{
        { { "group", { true, "set hook group, see remove-hooks" } },
          { "always", { false, "run hook even if hooks are disabled" } },
          { "once", { false, "run the hook only once" } } },
        ParameterDesc::Flags::None, 4, 4
    },
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(complete_scope), menu(complete_hooks), complete_nothing,
                   [](const Context& context, CompletionFlags flags,
                      StringView prefix, ByteCount cursor_pos)
                   { return CommandManager::instance().complete(
                         context, flags, prefix, cursor_pos); }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        auto descs = enum_desc(Meta::Type<Hook>{});
        auto it = find_if(descs, [&](const EnumDesc<Hook>& desc) { return desc.name == parser[1]; });
        if (it == descs.end())
            throw runtime_error{format("no such hook: '{}'", parser[1])};

        Regex regex{parser[2], RegexCompileFlags::Optimize};
        const String& command = parser[3];
        auto group = parser.get_switch("group").value_or(StringView{});

        if (any_of(group, [](char c) { return not is_word(c, { '-' }); }) or
            (not group.empty() and not is_word(group[0])))
            throw runtime_error{format("invalid group name '{}'", group)};

        const auto flags = (parser.get_switch("always") ? HookFlags::Always : HookFlags::None) |
                           (parser.get_switch("once")   ? HookFlags::Once   : HookFlags::None);
        get_scope(parser[0], context).hooks().add_hook(it->value, group.str(), flags,
                                                       std::move(regex), command);
    }
};

const CommandDesc remove_hook_cmd = {
    "remove-hooks",
    "rmhooks",
    "remove-hooks <scope> <group>: remove all hooks whose group matches the regex <group>",
    double_params,
    CommandFlags::None,
    CommandHelper{},
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return menu(complete_scope)(context, flags, params[0], pos_in_token);
        else if (token_to_complete == 1)
        {
            if (auto scope = get_scope_ifp(params[0], context))
                return { 0_byte, params[0].length(),
                         scope->hooks().complete_hook_group(params[1], pos_in_token),
                         Completions::Flags::Menu };
        }
        return {};
    },
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        get_scope(parser[0], context).hooks().remove_hooks(Regex{parser[1]});
    }
};

const CommandDesc trigger_user_hook_cmd = {
    "trigger-user-hook",
    nullptr,
    "trigger-user-hook <param>: run 'User' hook with <param> as filter string",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        context.hooks().run_hook(Hook::User, parser[0], context);
    }
};

Vector<String> params_to_shell(const ParametersParser& parser)
{
    Vector<String> vars;
    for (size_t i = 0; i < parser.positional_count(); ++i)
        vars.push_back(parser[i]);
    return vars;
}

CommandCompleter make_command_completer(StringView type, StringView param, Completions::Flags completions_flags)
{
    if (type == "file")
    {
        return [=](const Context& context, CompletionFlags flags,
                   CommandParameters params,
                   size_t token_to_complete, ByteCount pos_in_token) {
             const String& prefix = params[token_to_complete];
             const auto& ignored_files = context.options()["ignored_files"].get<Regex>();
             return Completions{0_byte, pos_in_token,
                                complete_filename(prefix, ignored_files,
                                                  pos_in_token, FilenameFlags::Expand),
                                completions_flags};
        };
    }
    else if (type == "client")
    {
        return [=](const Context& context, CompletionFlags flags,
                   CommandParameters params,
                   size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = params[token_to_complete];
             auto& cm = ClientManager::instance();
             return Completions{0_byte, pos_in_token,
                                cm.complete_client_name(prefix, pos_in_token),
                                completions_flags};
        };
    }
    else if (type == "buffer")
    {
        return [=](const Context& context, CompletionFlags flags,
                   CommandParameters params,
                   size_t token_to_complete, ByteCount pos_in_token)
        {
             return add_flags(complete_buffer_name<false>, completions_flags)(
                 context, flags, params[token_to_complete], pos_in_token);
        };
    }
    else if (type == "shell-script")
    {
        if (param.empty())
            throw runtime_error("shell-script requires a shell script parameter");

        return ShellScriptCompleter{param.str(), completions_flags};
    }
    else if (type == "shell-script-candidates")
    {
        if (param.empty())
            throw runtime_error("shell-script-candidates requires a shell script parameter");

        return ShellCandidatesCompleter{param.str(), completions_flags};
    }
    else if (type == "command")
    {
        return [](const Context& context, CompletionFlags flags,
                  CommandParameters params,
                  size_t token_to_complete, ByteCount pos_in_token)
        {
            return CommandManager::instance().complete(
                context, flags, params, token_to_complete, pos_in_token);
        };
    }
    else if (type == "shell")
    {
        return [=](const Context& context, CompletionFlags flags,
                   CommandParameters params,
                   size_t token_to_complete, ByteCount pos_in_token)
        {
            return add_flags(shell_complete, completions_flags)(
                context, flags, params[token_to_complete], pos_in_token);
        };
    }
    else
        throw runtime_error(format("invalid command completion type '{}'", type));
}

static CommandCompleter parse_completion_switch(const ParametersParser& parser, Completions::Flags completions_flags) {
    for (StringView completion_switch : {"file-completion", "client-completion", "buffer-completion",
                                         "shell-script-completion", "shell-script-candidates",
                                         "command-completion", "shell-completion"})
    {
        if (auto param = parser.get_switch(completion_switch))
        {
            constexpr StringView suffix = "-completion";
            if (completion_switch.ends_with(suffix))
                completion_switch = completion_switch.substr(0, completion_switch.length() - suffix.length());
            return make_command_completer(completion_switch, *param, completions_flags);
        }
    }
    return {};
}

void define_command(const ParametersParser& parser, Context& context, const ShellContext&)
{
    const String& cmd_name = parser[0];
    auto& cm = CommandManager::instance();

    if (not all_of(cmd_name, is_identifier))
        throw runtime_error(format("invalid command name: '{}'", cmd_name));

    if (cm.command_defined(cmd_name) and not parser.get_switch("override"))
        throw runtime_error(format("command '{}' already defined", cmd_name));

    CommandFlags flags = CommandFlags::None;
    if (parser.get_switch("hidden"))
        flags = CommandFlags::Hidden;

    const Completions::Flags completions_flags = parser.get_switch("menu") ?
        Completions::Flags::Menu : Completions::Flags::None;

    const String& commands = parser[1];
    CommandFunc cmd;
    ParameterDesc desc;
    if (auto params = parser.get_switch("params"))
    {
        size_t min = 0, max = -1;
        StringView counts = *params;
        static const Regex re{R"((\d+)?..(\d+)?)"};
        MatchResults<const char*> res;
        if (regex_match(counts.begin(), counts.end(), res, re))
        {
            if (res[1].matched)
                min = (size_t)str_to_int({res[1].first, res[1].second});
            if (res[2].matched)
                max = (size_t)str_to_int({res[2].first, res[2].second});
        }
        else
            min = max = (size_t)str_to_int(counts);

        desc = ParameterDesc{ {}, ParameterDesc::Flags::SwitchesAsPositional, min, max };
        cmd = [=](const ParametersParser& parser, Context& context, const ShellContext& sc) {
            CommandManager::instance().execute(commands, context,
                                               { params_to_shell(parser), sc.env_vars });
        };
    }
    else
    {
        desc = ParameterDesc{ {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 0 };
        cmd = [=](const ParametersParser& parser, Context& context, const ShellContext& sc) {
            CommandManager::instance().execute(commands, context, { {}, sc.env_vars });
        };
    }

    CommandCompleter completer = parse_completion_switch(parser, completions_flags);
    auto docstring = trim_indent(parser.get_switch("docstring").value_or(StringView{}));

    cm.register_command(cmd_name, cmd, docstring, desc, flags, CommandHelper{}, std::move(completer));
}

const CommandDesc define_command_cmd = {
    "define-command",
    "def",
    "define-command [<switches>] <name> <cmds>: define a command <name> executing <cmds>",
    ParameterDesc{
        { { "params",                   { true,  "take parameters, accessible to each shell escape as $0..$N\n"
                                                 "parameter should take the form <count> or <min>..<max> (both omittable)" } },
          { "override",                 { false, "allow overriding an existing command" } },
          { "hidden",                   { false, "do not display the command in completion candidates" } },
          { "docstring",                { true,  "define the documentation string for command" } },
          { "menu",                     { false, "treat completions as the only valid inputs" } },
          { "file-completion",          { false, "complete parameters using filename completion" } },
          { "client-completion",        { false, "complete parameters using client name completion" } },
          { "buffer-completion",        { false, "complete parameters using buffer name completion" } },
          { "command-completion",       { false, "complete parameters using kakoune command completion" } },
          { "shell-completion",         { false, "complete parameters using shell command completion" } },
          { "shell-script-completion",  { true,  "complete parameters using the given shell-script" } },
          { "shell-script-candidates",  { true,  "get the parameter candidates using the given shell-script" } } },
        ParameterDesc::Flags::None,
        2, 2
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    define_command
};

static Completions complete_alias_name(const Context& context, CompletionFlags,
                                       StringView prefix, ByteCount cursor_pos)
{
   return { 0_byte, cursor_pos, complete(prefix, cursor_pos,
                                           context.aliases().flatten_aliases()
                                         | transform(&HashItem<String, String>::key))};
}

const CommandDesc alias_cmd = {
    "alias",
    nullptr,
    "alias <scope> <alias> <command>: alias <alias> to <command> in <scope>",
    ParameterDesc{{}, ParameterDesc::Flags::None, 3, 3},
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(complete_scope), complete_alias_name, complete_command_name),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not CommandManager::instance().command_defined(parser[2]))
            throw runtime_error(format("no such command: '{}'", parser[2]));

        AliasRegistry& aliases = get_scope(parser[0], context).aliases();
        aliases.add_alias(parser[1], parser[2]);
    }
};

const CommandDesc unalias_cmd = {
    "unalias",
    nullptr,
    "unalias <scope> <alias> [<expected>]: remove <alias> from <scope>\n"
    "If <expected> is specified, remove <alias> only if its value is <expected>",
    ParameterDesc{{}, ParameterDesc::Flags::None, 2, 3},
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(complete_scope), complete_alias_name, complete_command_name),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        AliasRegistry& aliases = get_scope(parser[0], context).aliases();
        if (parser.positional_count() == 3 and
            aliases[parser[1]] != parser[2])
            return;
        aliases.remove_alias(parser[1]);
    }
};

const CommandDesc complete_command_cmd = {
    "complete-command",
    "compl",
    "complete-command [<switches>] <name> <type> [<param>]\n"
    "define command completion",
    ParameterDesc{
        { { "menu",                     { false, "treat completions as the only valid inputs" } }, },
        ParameterDesc::Flags::None, 2, 3},
    CommandFlags::None,
    CommandHelper{},
    make_completer(complete_command_name),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        const Completions::Flags flags = parser.get_switch("menu") ? Completions::Flags::Menu : Completions::Flags::None;
        CommandCompleter completer = make_command_completer(parser[1], parser.positional_count() >= 3 ? parser[2] : StringView{}, flags);
        CommandManager::instance().set_command_completer(parser[0], std::move(completer));
    }
};

const CommandDesc echo_cmd = {
    "echo",
    nullptr,
    "echo <params>...: display given parameters in the status line",
    ParameterDesc{
        { { "markup", { false, "parse markup" } },
          { "quoting", { true, "quote each argument separately using the given style (raw|kakoune|shell)" } },
          { "to-file", { true, "echo contents to given filename" } },
          { "debug", { false, "write to debug buffer instead of status line" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        String message;
        if (auto quoting = parser.get_switch("quoting"))
            message = join(parser | transform(quoter(option_from_string(Meta::Type<Quoting>{}, *quoting))),
                           ' ', false);
        else
            message = join(parser, ' ', false);

        if (auto filename = parser.get_switch("to-file"))
            return write_to_file(*filename, message);

        if (parser.get_switch("debug"))
            write_to_debug_buffer(message);
        else if (parser.get_switch("markup"))
            context.print_status(parse_display_line(message, context.faces()));
        else
            context.print_status({message, context.faces()["StatusLine"]});
    }
};

KeymapMode parse_keymap_mode(StringView str, const KeymapManager::UserModeList& user_modes)
{
    if (prefix_match("normal", str)) return KeymapMode::Normal;
    if (prefix_match("insert", str)) return KeymapMode::Insert;
    if (prefix_match("menu", str))   return KeymapMode::Menu;
    if (prefix_match("prompt", str)) return KeymapMode::Prompt;
    if (prefix_match("goto", str))   return KeymapMode::Goto;
    if (prefix_match("view", str))   return KeymapMode::View;
    if (prefix_match("user", str))   return KeymapMode::User;
    if (prefix_match("object", str)) return KeymapMode::Object;

    auto it = find(user_modes, str);
    if (it == user_modes.end())
        throw runtime_error(format("no such keymap mode: '{}'", str));

    char offset = static_cast<char>(KeymapMode::FirstUserMode);
    return (KeymapMode)(std::distance(user_modes.begin(), it) + offset);
}

static constexpr auto modes = make_array<StringView>({ "normal", "insert", "menu", "prompt", "goto", "view", "user", "object" });

const CommandDesc debug_cmd = {
    "debug",
    nullptr,
    "debug <command>: write some debug information to the *debug* buffer",
    ParameterDesc{{}, ParameterDesc::Flags::SwitchesOnlyAtStart, 1},
    CommandFlags::None,
    CommandHelper{},
    make_completer(
        [](const Context& context, CompletionFlags flags,
           StringView prefix, ByteCount cursor_pos) -> Completions {
               auto c = {"info", "buffers", "options", "memory", "shared-strings",
                         "profile-hash-maps", "faces", "mappings", "regex", "registers"};
               return { 0_byte, cursor_pos, complete(prefix, cursor_pos, c), Completions::Flags::Menu };
    }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (parser[0] == "info")
        {
            write_to_debug_buffer(format("version: {}", version));
            write_to_debug_buffer(format("pid: {}", getpid()));
            write_to_debug_buffer(format("session: {}", Server::instance().session()));
            #ifdef KAK_DEBUG
            write_to_debug_buffer("build: debug");
            #else
            write_to_debug_buffer("build: release");
            #endif
        }
        else if (parser[0] == "buffers")
        {
            write_to_debug_buffer("Buffers:");
            for (auto& buffer : BufferManager::instance())
                write_to_debug_buffer(buffer->debug_description());
        }
        else if (parser[0] == "options")
        {
            write_to_debug_buffer("Options:");
            for (auto& option : context.options().flatten_options())
                write_to_debug_buffer(format(" * {}: {}", option->name(),
                                             option->get_as_string(Quoting::Kakoune)));
        }
        else if (parser[0] == "memory")
        {
            auto total = 0;
            write_to_debug_buffer("Memory usage:");
            const ColumnCount column_size = 17;
            write_to_debug_buffer(format("{:17} │{:17} │{:17} │{:17} ",
                                         "domain",
                                         "bytes",
                                         "active allocs",
                                         "total allocs"));
            write_to_debug_buffer(format("{0}┼{0}┼{0}┼{0}", String(Codepoint{0x2500}, column_size + 1)));

            for (int domain = 0; domain < (int)MemoryDomain::Count; ++domain)
            {
                auto& stats = memory_stats[domain];
                total += stats.allocated_bytes;
                write_to_debug_buffer(format("{:17} │{:17} │{:17} │{:17} ",
                                             domain_name((MemoryDomain)domain),
                                             grouped(stats.allocated_bytes),
                                             grouped(stats.allocation_count),
                                             grouped(stats.total_allocation_count)));
            }
            write_to_debug_buffer({});
            write_to_debug_buffer(format("  Total: {}", grouped(total)));
            #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
            write_to_debug_buffer(format("  Malloced: {}", grouped(mallinfo2().uordblks)));
            #elif defined(__GLIBC__) || defined(__CYGWIN__)
            write_to_debug_buffer(format("  Malloced: {}", grouped(mallinfo().uordblks)));
            #endif
        }
        else if (parser[0] == "shared-strings")
        {
            StringRegistry::instance().debug_stats();
        }
        else if (parser[0] == "profile-hash-maps")
        {
            profile_hash_maps();
        }
        else if (parser[0] == "faces")
        {
            write_to_debug_buffer("Faces:");
            for (auto& face : context.faces().flatten_faces())
                write_to_debug_buffer(format(" * {}: {}", face.key, face.value.face));
        }
        else if (parser[0] == "mappings")
        {
            auto& keymaps = context.keymaps();
            auto user_modes = keymaps.user_modes();
            write_to_debug_buffer("Mappings:");
            for (auto mode : concatenated(modes, user_modes))
            {
                KeymapMode m = parse_keymap_mode(mode, user_modes);
                for (auto& key : keymaps.get_mapped_keys(m))
                    write_to_debug_buffer(format(" * {} {}: {}",
                                          mode, key, keymaps.get_mapping(key, m).docstring));
            }
        }
        else if (parser[0] == "regex")
        {
            if (parser.positional_count() != 2)
                throw runtime_error("expected a regex");

            write_to_debug_buffer(format(" * {}:\n{}",
                                  parser[1], dump_regex(compile_regex(parser[1], RegexCompileFlags::Optimize))));
        }
        else if (parser[0] == "registers")
        {
            write_to_debug_buffer("Register info:");
            for (auto&& [name, reg] : RegisterManager::instance())
            {
                auto content = reg->get(context);

                if (content.size() == 1 and content[0] == "")
                    continue;

                write_to_debug_buffer(format(" * {} = {}\n", name,
                    join(content | transform(quote), "\n     = ")));
            }
        }
        else
            throw runtime_error(format("no such debug command: '{}'", parser[0]));
    }
};

const CommandDesc source_cmd = {
    "source",
    nullptr,
    "source <filename> <params>...: execute commands contained in <filename>\n"
    "parameters are available in the sourced script as %arg{0}, %arg{1}, …",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 1, (size_t)-1 },
    CommandFlags::None,
    CommandHelper{},
    filename_completer<true>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        const DebugFlags debug_flags = context.options()["debug"].get<DebugFlags>();
        const bool profile = debug_flags & DebugFlags::Profile;
        auto start_time = profile ? Clock::now() : Clock::time_point{};

        String path = real_path(parse_filename(parser[0]));
        MappedFile file_content{path};
        try
        {
            auto params = parser | skip(1) | gather<Vector<String>>();
            CommandManager::instance().execute(file_content, context,
                                               {params, {{"source", path}}});
        }
        catch (Kakoune::runtime_error& err)
        {
            write_to_debug_buffer(format("{}:{}", parser[0], err.what()));
            throw;
        }

        using namespace std::chrono;
        if (profile)
            write_to_debug_buffer(format("sourcing '{}' took {} us", parser[0],
                                         (size_t)duration_cast<microseconds>(Clock::now() - start_time).count()));
    }
};

static String option_doc_helper(const Context& context, CommandParameters params)
{
    const bool is_switch = params.size() > 1 and (params[0] == "-add" or params[0] == "-remove");
    if (params.size() < 2 + (is_switch ? 1 : 0))
        return "";

    auto desc = GlobalScope::instance().option_registry().option_desc(params[1 + (is_switch ? 1 : 0)]);
    if (not desc or desc->docstring().empty())
        return "";

    return format("{}:\n{}", desc->name(), indent(desc->docstring()));
}

static OptionManager& get_options(StringView scope, const Context& context, StringView option_name)
{
    if (scope == "current")
        return context.options()[option_name].manager();
    return get_scope(scope, context).options();
}

const CommandDesc set_option_cmd = {
    "set-option",
    "set",
    "set-option [<switches>] <scope> <name> <value>: set option <name> in <scope> to <value>\n"
    "<scope> can be global, buffer, window, or current which refers to the narrowest "
    "scope the option is set in",
    ParameterDesc{
        { { "add",    { false, "add to option rather than replacing it" } },
          { "remove", { false, "remove from option rather than replacing it" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 2, (size_t)-1
    },
    CommandFlags::None,
    option_doc_helper,
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return menu(complete_scope_including_current)(context, flags, params[0], pos_in_token);
        else if (token_to_complete == 1)
            return { 0_byte, params[1].length(),
                     GlobalScope::instance().option_registry().complete_option_name(params[1], pos_in_token),
                     Completions::Flags::Menu };
        else if (token_to_complete == 2  and params[2].empty() and
                 GlobalScope::instance().option_registry().option_exists(params[1]))
        {
            OptionManager& options = get_scope(params[0], context).options();
            return {0_byte, params[2].length(),
                    {options[params[1]].get_as_string(Quoting::Kakoune)},
                    Completions::Flags::Quoted};
        }
        return Completions{};
    },
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        bool add = (bool)parser.get_switch("add");
        bool remove = (bool)parser.get_switch("remove");
        if (add and remove)
            throw runtime_error("cannot add and remove at the same time");

        Option& opt = get_options(parser[0], context, parser[1]).get_local_option(parser[1]);
        if (add)
            opt.add_from_strings(parser.positionals_from(2));
        else if (remove)
            opt.remove_from_strings(parser.positionals_from(2));
        else
            opt.set_from_strings(parser.positionals_from(2));
    }
};

Completions complete_option(const Context& context, CompletionFlags flags,
                            CommandParameters params, size_t token_to_complete,
                            ByteCount pos_in_token)
{
    if (token_to_complete == 0)
        return menu(complete_scope_no_global)(context, flags, params[0], pos_in_token);
    else if (token_to_complete == 1)
        return { 0_byte, params[1].length(),
                 GlobalScope::instance().option_registry().complete_option_name(params[1], pos_in_token),
                 Completions::Flags::Menu };
    return Completions{};
}

const CommandDesc unset_option_cmd = {
    "unset-option",
    "unset",
    "unset-option <scope> <name>: remove <name> option from scope, falling back on parent scope value\n"
    "<scope> can be buffer, window, or current which refers to the narrowest "
    "scope the option is set in",
    double_params,
    CommandFlags::None,
    option_doc_helper,
    complete_option,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        auto& options = get_options(parser[0], context, parser[1]);
        if (&options == &GlobalScope::instance().options())
            throw runtime_error("cannot unset options in global scope");
        options.unset_option(parser[1]);
    }
};

const CommandDesc update_option_cmd = {
    "update-option",
    nullptr,
    "update-option <scope> <name>: update <name> option from scope\n"
    "some option types, such as line-specs or range-specs can be updated to latest buffer timestamp\n"
    "<scope> can be buffer, window, or current which refers to the narrowest "
    "scope the option is set in",
    double_params,
    CommandFlags::None,
    option_doc_helper,
    complete_option,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        Option& opt = get_options(parser[0], context, parser[1]).get_local_option(parser[1]);
        opt.update(context);
    }
};

const CommandDesc declare_option_cmd = {
    "declare-option",
    "decl",
    "declare-option [<switches>] <type> <name> [value]: declare option <name> of type <type>.\n"
    "set its initial value to <value> if given and the option did not exist\n"
    "Available types:\n"
    "    int: integer\n"
    "    bool: boolean (true/false or yes/no)\n"
    "    str: character string\n"
    "    regex: regular expression\n"
    "    int-list: list of integers\n"
    "    str-list: list of character strings\n"
    "    completions: list of completion candidates\n"
    "    line-specs: list of line specs\n"
    "    range-specs: list of range specs\n"
    "    str-to-str-map: map from strings to strings\n",
    ParameterDesc{
        { { "hidden",    { false, "do not display option name when completing" } },
          { "docstring", { true,  "specify option description" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 2, (size_t)-1
    },
    CommandFlags::None,
    CommandHelper{},
    make_completer(
        [](const Context& context, CompletionFlags flags,
           StringView prefix, ByteCount cursor_pos) -> Completions {
               auto c = {"int", "bool", "str", "regex", "int-list", "str-list", "completions", "line-specs", "range-specs", "str-to-str-map"};
               return { 0_byte, cursor_pos, complete(prefix, cursor_pos, c), Completions::Flags::Menu };
    }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        Option* opt = nullptr;

        OptionFlags flags = OptionFlags::None;
        if (parser.get_switch("hidden"))
            flags = OptionFlags::Hidden;

        auto docstring = trim_indent(parser.get_switch("docstring").value_or(StringView{}));
        OptionsRegistry& reg = GlobalScope::instance().option_registry();


        if (parser[0] == "int")
            opt = &reg.declare_option<int>(parser[1], docstring, 0, flags);
        else if (parser[0] == "bool")
            opt = &reg.declare_option<bool>(parser[1], docstring, false, flags);
        else if (parser[0] == "str")
            opt = &reg.declare_option<String>(parser[1], docstring, "", flags);
        else if (parser[0] == "regex")
            opt = &reg.declare_option<Regex>(parser[1], docstring, Regex{}, flags);
        else if (parser[0] == "int-list")
            opt = &reg.declare_option<Vector<int, MemoryDomain::Options>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "str-list")
            opt = &reg.declare_option<Vector<String, MemoryDomain::Options>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "completions")
            opt = &reg.declare_option<CompletionList>(parser[1], docstring, {}, flags);
        else if (parser[0] == "line-specs")
            opt = &reg.declare_option<TimestampedList<LineAndSpec>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "range-specs")
            opt = &reg.declare_option<TimestampedList<RangeAndString>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "str-to-str-map")
            opt = &reg.declare_option<HashMap<String, String, MemoryDomain::Options>>(parser[1], docstring, {}, flags);
        else
            throw runtime_error(format("no such option type: '{}'", parser[0]));

        if (parser.positional_count() > 2)
            opt->set_from_strings(parser.positionals_from(2));
    }
};

template<bool unmap>
static Completions map_key_completer(const Context& context, CompletionFlags flags,
                                     CommandParameters params, size_t token_to_complete,
                                     ByteCount pos_in_token)
{
    if (token_to_complete == 0)
        return menu(complete_scope)(context, flags, params[0], pos_in_token);
    if (token_to_complete == 1)
    {
        auto& user_modes = get_scope(params[0], context).keymaps().user_modes();
        return { 0_byte, params[1].length(),
                 complete(params[1], pos_in_token, concatenated(modes, user_modes)),
                 Completions::Flags::Menu };
    }
    if (unmap and token_to_complete == 2)
    {
        KeymapManager& keymaps = get_scope(params[0], context).keymaps();
        KeymapMode keymap_mode = parse_keymap_mode(params[1], keymaps.user_modes());
        KeyList keys = keymaps.get_mapped_keys(keymap_mode);

        return { 0_byte, params[2].length(),
                 complete(params[2], pos_in_token,
                          keys | transform([](Key k) { return to_string(k); })
                               | gather<Vector<String>>()),
                 Completions::Flags::Menu };
    }
    return {};
}

const CommandDesc map_key_cmd = {
    "map",
    nullptr,
    "map [<switches>] <scope> <mode> <key> <keys>: map <key> to <keys> in given <mode> in <scope>",
    ParameterDesc{
        { { "docstring", { true,  "specify mapping description" } } },
        ParameterDesc::Flags::None, 4, 4
    },
    CommandFlags::None,
    CommandHelper{},
    map_key_completer<false>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        KeymapManager& keymaps = get_scope(parser[0], context).keymaps();
        KeymapMode keymap_mode = parse_keymap_mode(parser[1], keymaps.user_modes());

        KeyList key = parse_keys(parser[2]);
        if (key.size() != 1)
            throw runtime_error("only a single key can be mapped");

        KeymapMode lower_case_only_modes[] = {KeymapMode::Goto};
        if (key[0].codepoint().map(iswupper).value_or(false) and
            contains(lower_case_only_modes, keymap_mode))
            throw runtime_error("mode only supports lower case mappings");

        KeyList mapping = parse_keys(parser[3]);
        keymaps.map_key(key[0], keymap_mode, std::move(mapping),
                        trim_indent(parser.get_switch("docstring").value_or("")));
    }
};

const CommandDesc unmap_key_cmd = {
    "unmap",
    nullptr,
    "unmap <scope> <mode> [<key> [<expected-keys>]]: unmap <key> from given <mode> in <scope>.\n"
    "If <expected-keys> is specified, remove the mapping only if its value is <expected-keys>.\n"
    "If only <scope> and <mode> are specified remove all mappings",
    ParameterDesc{{}, ParameterDesc::Flags::None, 2, 4},
    CommandFlags::None,
    CommandHelper{},
    map_key_completer<true>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        KeymapManager& keymaps = get_scope(parser[0], context).keymaps();
        KeymapMode keymap_mode = parse_keymap_mode(parser[1], keymaps.user_modes());

        if (parser.positional_count() == 2)
        {
            keymaps.unmap_keys(keymap_mode);
            return;
        }

        KeyList key = parse_keys(parser[2]);
        if (key.size() != 1)
            throw runtime_error("only a single key can be unmapped");

        if (keymaps.is_mapped(key[0], keymap_mode) and
            (parser.positional_count() < 4 or
             (keymaps.get_mapping(key[0], keymap_mode).keys ==
              parse_keys(parser[3]))))
            keymaps.unmap_key(key[0], keymap_mode);
    }
};

template<size_t... P>
ParameterDesc make_context_wrap_params_impl(Array<HashItem<String, SwitchDesc>, sizeof...(P)>&& additional_params,
                                            std::index_sequence<P...>)
{
    return { { { "client",     { true,  "run in given client context" } },
               { "try-client", { true,  "run in given client context if it exists, or else in the current one" } },
               { "buffer",     { true,  "run in a disposable context for each given buffer in the comma separated list argument" } },
               { "draft",      { false, "run in a disposable context" } },
               { "itersel",    { false, "run once for each selection with that selection as the only one" } },
               std::move(additional_params[P])...},
        ParameterDesc::Flags::SwitchesOnlyAtStart, 1
    };
}

template<size_t N>
ParameterDesc make_context_wrap_params(Array<HashItem<String, SwitchDesc>, N>&& additional_params)
{
    return make_context_wrap_params_impl(std::move(additional_params), std::make_index_sequence<N>());
}

template<typename Func>
void context_wrap(const ParametersParser& parser, Context& context, StringView default_saved_regs, Func func)
{
    if ((int)(bool)parser.get_switch("buffer") +
        (int)(bool)parser.get_switch("client") +
        (int)(bool)parser.get_switch("try-client") > 1)
        throw runtime_error{"only one of -buffer, -client or -try-client can be specified"};

    const auto& register_manager = RegisterManager::instance();
    auto make_register_restorer = [&](char c) {
        auto& reg = register_manager[c];
        return on_scope_end([&, c, save=reg.save(context), d=ScopedSetBool{reg.modified_hook_disabled()}] {
            try
            {
                reg.restore(context, save);
            }
            catch (runtime_error& err)
            {
                write_to_debug_buffer(format("failed to restore register '{}': {}", c, err.what()));
            }
        });
    };
    Vector<decltype(make_register_restorer(0))> saved_registers;
    for (auto c : parser.get_switch("save-regs").value_or(default_saved_regs))
        saved_registers.push_back(make_register_restorer(c));

    if (auto bufnames = parser.get_switch("buffer"))
    {
        auto context_wrap_for_buffer = [&](Buffer& buffer) {
            InputHandler input_handler{{ buffer, Selection{} },
                                       Context::Flags::Draft};
            Context& c = input_handler.context();

            ScopedSetBool disable_history(c.history_disabled());

            func(parser, c);
        };
        if (*bufnames == "*")
        {
            for (auto&& buffer : BufferManager::instance()
                               | transform(&std::unique_ptr<Buffer>::get)
                               | filter([](Buffer* buf) { return not (buf->flags() & Buffer::Flags::Debug); })
                               | gather<Vector<SafePtr<Buffer>>>()) // gather as we might be mutating the buffer list in the loop.
                context_wrap_for_buffer(*buffer);
        }
        else
            for (auto&& name : *bufnames | split<StringView>(','))
                context_wrap_for_buffer(BufferManager::instance().get_buffer(name));
        return;
    }

    ClientManager& cm = ClientManager::instance();
    Context* base_context = &context;
    if (auto client_name = parser.get_switch("client"))
        base_context = &cm.get_client(*client_name).context();
    else if (auto client_name = parser.get_switch("try-client"))
    {
        if (Client* client = cm.get_client_ifp(*client_name))
            base_context = &client->context();
    }

    Optional<InputHandler> input_handler;
    Context* effective_context = base_context;

    const bool draft = (bool)parser.get_switch("draft");
    if (draft)
    {
        input_handler.emplace(base_context->selections(),
                              Context::Flags::Draft,
                              base_context->name());
        effective_context = &input_handler->context();

        // Preserve window so that window scope is available
        if (base_context->has_window())
            effective_context->set_window(base_context->window());

        // We do not want this draft context to commit undo groups if the real one is
        // going to commit the whole thing later
        if (base_context->is_editing())
            effective_context->disable_undo_handling();
    }

    Context& c = *effective_context;

    ScopedSetBool disable_history(c.history_disabled());
    ScopedEdition edition{c};

    if (parser.get_switch("itersel"))
    {
        SelectionList sels{base_context->selections()};
        Vector<Selection> new_sels;
        size_t main = 0;
        size_t timestamp = c.buffer().timestamp();
        bool one_selection_succeeded = false;
        for (auto& sel : sels)
        {
            c.selections_write_only() = SelectionList{sels.buffer(), sel, sels.timestamp()};
            c.selections().update();

            try
            {
                func(parser, c);
                one_selection_succeeded = true;

                if (&sels.buffer() != &c.buffer())
                    throw runtime_error("buffer has changed while iterating on selections");

                if (not draft)
                {
                    update_selections(new_sels, main, c.buffer(), timestamp);
                    timestamp = c.buffer().timestamp();
                    if (&sel == &sels.main())
                        main = new_sels.size() + c.selections().main_index();

                    const auto middle = new_sels.insert(new_sels.end(), c.selections().begin(), c.selections().end());
                    std::inplace_merge(new_sels.begin(), middle, new_sels.end(), compare_selections);
                }
            }
            catch (no_selections_remaining&) {}
        }

        if (not one_selection_succeeded)
        {
            c.selections_write_only() = std::move(sels);
            throw no_selections_remaining{};
        }

        if (not draft)
            c.selections_write_only().set(std::move(new_sels), main);
    }
    else
    {
        const bool collapse_jumps = not (c.flags() & Context::Flags::Draft) and context.has_buffer();
        auto& jump_list = c.jump_list();
        const size_t prev_index = jump_list.current_index();
        auto jump = collapse_jumps ? c.selections() : Optional<SelectionList>{};

        func(parser, c);

        // If the jump list got mutated, collapse all jumps into a single one from original selections
        if (auto index = jump_list.current_index();
            collapse_jumps and index > prev_index and
            contains(BufferManager::instance(), &jump->buffer()))
            jump_list.push(std::move(*jump), prev_index);
    }
}

const CommandDesc execute_keys_cmd = {
    "execute-keys",
    "exec",
    "execute-keys [<switches>] <keys>: execute given keys as if entered by user",
    make_context_wrap_params<3>({{
        {"save-regs",  {true, "restore all given registers after execution (default: '/\"|^@:')"}},
        {"with-maps",  {false, "use user defined key mapping when executing keys"}},
        {"with-hooks", {false, "trigger hooks while executing keys"}}
    }}),
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        context_wrap(parser, context, "/\"|^@:", [](const ParametersParser& parser, Context& context) {
            ScopedSetBool disable_keymaps(context.keymaps_disabled(), not parser.get_switch("with-maps"));
            ScopedSetBool disable_hooks(context.hooks_disabled(), not parser.get_switch("with-hooks"));

            for (auto& key : parser | transform(parse_keys) | flatten())
                context.input_handler().handle_key(key);
        });
    }
};

const CommandDesc evaluate_commands_cmd = {
    "evaluate-commands",
    "eval",
    "evaluate-commands [<switches>] <commands>...: execute commands as if entered by user",
    make_context_wrap_params<3>({{
        {"save-regs",  {true, "restore all given registers after execution (default: '')"}},
        {"no-hooks", { false, "disable hooks while executing commands" }},
        {"verbatim", { false, "do not reparse argument" }}
    }}),
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        context_wrap(parser, context, {}, [&](const ParametersParser& parser, Context& context) {
            const bool no_hooks = context.hooks_disabled() or parser.get_switch("no-hooks");
            ScopedSetBool disable_hooks(context.hooks_disabled(), no_hooks);

            if (parser.get_switch("verbatim"))
                CommandManager::instance().execute_single_command(parser | gather<Vector>(), context, shell_context);
            else
                CommandManager::instance().execute(join(parser, ' ', false), context, shell_context);
        });
    }
};

struct CapturedShellContext
{
    explicit CapturedShellContext(const ShellContext& sc)
      : params{sc.params.begin(), sc.params.end()}, env_vars{sc.env_vars} {}

    Vector<String> params;
    EnvVarMap env_vars;

    operator ShellContext() const { return { params, env_vars }; }
};

const CommandDesc prompt_cmd = {
    "prompt",
    nullptr,
    "prompt [<switches>] <prompt> <command>: prompt the user to enter a text string "
    "and then executes <command>, entered text is available in the 'text' value",
    ParameterDesc{
        { { "init", { true, "set initial prompt content" } },
          { "password", { false, "Do not display entered text and clear reg after command" } },
          { "menu", { false, "treat completions as the only valid inputs" } },
          { "file-completion", { false, "use file completion for prompt" } },
          { "client-completion", { false, "use client completion for prompt" } },
          { "buffer-completion", { false, "use buffer completion for prompt" } },
          { "command-completion", { false, "use command completion for prompt" } },
          { "shell-completion", { false, "use shell command completion for prompt" } },
          { "shell-script-completion", { true, "use shell command completion for prompt" } },
          { "shell-script-candidates", { true, "use shell command completion for prompt" } },
          { "on-change", { true, "command to execute whenever the prompt changes" } },
          { "on-abort", { true, "command to execute whenever the prompt is canceled" } } },
        ParameterDesc::Flags::None, 2, 2
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        const String& command = parser[1];
        auto initstr = parser.get_switch("init").value_or(StringView{});

        const Completions::Flags completions_flags = parser.get_switch("menu") ?
            Completions::Flags::Menu : Completions::Flags::None;
        PromptCompleterAdapter completer = parse_completion_switch(parser, completions_flags);

        const auto flags = parser.get_switch("password") ?
            PromptFlags::Password : PromptFlags::None;

        context.input_handler().prompt(
            parser[0], initstr.str(), {}, context.faces()["Prompt"],
            flags, '_', std::move(completer),
            [command,
             on_change = parser.get_switch("on-change").value_or("").str(),
             on_abort = parser.get_switch("on-abort").value_or("").str(),
             sc = CapturedShellContext{shell_context}]
            (StringView str, PromptEvent event, Context& context) mutable
            {
                if ((event == PromptEvent::Abort and on_abort.empty()) or
                    (event == PromptEvent::Change and on_change.empty()))
                    return;

                sc.env_vars["text"_sv] = String{String::NoCopy{}, str};
                auto remove_text = on_scope_end([&] {
                    sc.env_vars.erase("text"_sv);
                });

                ScopedSetBool disable_history{context.history_disabled()};

                StringView cmd;
                switch (event)
                {
                    case PromptEvent::Validate: cmd = command; break;
                    case PromptEvent::Change: cmd = on_change; break;
                    case PromptEvent::Abort: cmd = on_abort; break;
                }
                try
                {
                    CommandManager::instance().execute(cmd, context, sc);
                }
                catch (Kakoune::runtime_error& error)
                {
                    context.print_status({error.what().str(), context.faces()["Error"]});
                    context.hooks().run_hook(Hook::RuntimeError, error.what(), context);
                }
            });
    }
};

const CommandDesc menu_cmd = {
    "menu",
    nullptr,
    "menu [<switches>] <name1> <commands1> <name2> <commands2>...: display a "
    "menu and execute commands for the selected item",
    ParameterDesc{
        { { "auto-single", { false, "instantly validate if only one item is available" } },
          { "select-cmds", { false, "each item specify an additional command to run when selected" } },
          { "markup", { false, "parse menu entries as markup text" } } }
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        const bool with_select_cmds = (bool)parser.get_switch("select-cmds");
        const bool markup = (bool)parser.get_switch("markup");
        const size_t modulo = with_select_cmds ? 3 : 2;

        const size_t count = parser.positional_count();
        if (count == 0 or (count % modulo) != 0)
            throw wrong_argument_count();

        if (count == modulo and parser.get_switch("auto-single"))
        {
            ScopedSetBool disable_history{context.history_disabled()};

            CommandManager::instance().execute(parser[1], context);
            return;
        }

        Vector<DisplayLine> choices;
        Vector<String> commands;
        Vector<String> select_cmds;
        for (int i = 0; i < count; i += modulo)
        {
            if (parser[i].empty())
                throw runtime_error(format("entry #{} is empty", i+1));

            choices.push_back(markup ? parse_display_line(parser[i], context.faces())
                                     : DisplayLine{ parser[i], {} });
            commands.push_back(parser[i+1]);
            if (with_select_cmds)
                select_cmds.push_back(parser[i+2]);
        }

        CapturedShellContext sc{shell_context};
        context.input_handler().menu(std::move(choices),
            [=](int choice, MenuEvent event, Context& context) {
                ScopedSetBool disable_history{context.history_disabled()};

                if (event == MenuEvent::Validate and choice >= 0 and choice < commands.size())
                  CommandManager::instance().execute(commands[choice], context, sc);
                if (event == MenuEvent::Select and choice >= 0 and choice < select_cmds.size())
                  CommandManager::instance().execute(select_cmds[choice], context, sc);
            });
    }
};

const CommandDesc on_key_cmd = {
    "on-key",
    nullptr,
    "on-key [<switches>] <command>: wait for next user key and then execute <command>, "
    "with key available in the `key` value",
    ParameterDesc{
        { { "mode-name", { true, "set mode name to use" } } },
        ParameterDesc::Flags::None, 1, 1
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        String command = parser[0];

        CapturedShellContext sc{shell_context};
        context.input_handler().on_next_key(
            parser.get_switch("mode-name").value_or("on-key"),
            KeymapMode::None, [=](Key key, Context& context) mutable {
            sc.env_vars["key"_sv] = to_string(key);
            ScopedSetBool disable_history{context.history_disabled()};

            CommandManager::instance().execute(command, context, sc);
        });
    }
};

const CommandDesc info_cmd = {
    "info",
    nullptr,
    "info [<switches>] <text>: display an info box containing <text>",
    ParameterDesc{
        { { "anchor", { true, "set info anchoring <line>.<column>" } },
          { "style", { true, "set info style (above, below, menu, modal)" } },
          { "markup", { false, "parse markup" } },
          { "title", { true, "set info title" } } },
        ParameterDesc::Flags::None, 0, 1
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not context.has_client())
            return;

        const InfoStyle style = parser.get_switch("style").map(
            [](StringView style) -> Optional<InfoStyle> {
                if (style == "above") return InfoStyle::InlineAbove;
                if (style == "below") return InfoStyle::InlineBelow;
                if (style == "menu") return InfoStyle::MenuDoc;
                if (style == "modal") return InfoStyle::Modal;
                throw runtime_error(format("invalid style: '{}'", style));
            }).value_or(parser.get_switch("anchor") ? InfoStyle::Inline : InfoStyle::Prompt);

        context.client().info_hide(style == InfoStyle::Modal);
        if (parser.positional_count() == 0)
            return;

        const BufferCoord pos = parser.get_switch("anchor").map(
            [](StringView anchor) {
                auto dot = find(anchor, '.');
                if (dot == anchor.end())
                    throw runtime_error("expected <line>.<column> for anchor");

                return BufferCoord{str_to_int({anchor.begin(), dot})-1,
                                   str_to_int({dot+1, anchor.end()})-1};
            }).value_or(BufferCoord{});

        auto title = parser.get_switch("title").value_or(StringView{});
        if (parser.get_switch("markup"))
            context.client().info_show(parse_display_line(title, context.faces()),
                                       parse_display_line_list(parser[0], context.faces()),
                                       pos, style);
        else
            context.client().info_show(title.str(), parser[0], pos, style);
    }
};

const CommandDesc try_catch_cmd = {
    "try",
    nullptr,
    "try <cmds> [catch <error_cmds>]...: execute <cmds> in current context.\n"
    "if an error is raised and <error_cmds> is specified, execute it and do\n"
    "not propagate that error. If <error_cmds> raises an error and another\n"
    "<error_cmds> is provided, execute this one and so-on\n",
    ParameterDesc{{}, ParameterDesc::Flags::None, 1},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        if ((parser.positional_count() % 2) != 1)
            throw wrong_argument_count();

        for (size_t i = 1; i < parser.positional_count(); i += 2)
        {
            if (parser[i] != "catch")
                throw runtime_error("usage: try <commands> [catch <on error commands>]...");
        }

        CommandManager& command_manager = CommandManager::instance();
        Optional<ShellContext> shell_context_with_error;
        for (size_t i = 0; i < parser.positional_count(); i += 2)
        {
            if (i == 0 or i < parser.positional_count() - 1)
            {
                try
                {
                    command_manager.execute(parser[i], context,
                                            shell_context_with_error.value_or(shell_context));
                    return;
                }
                catch (const runtime_error& error)
                {
                    shell_context_with_error.emplace(shell_context);
                    shell_context_with_error->env_vars[StringView{"error"}] = error.what().str();
                }
            }
            else
                command_manager.execute(parser[i], context,
                                        shell_context_with_error.value_or(shell_context));
        }
    }
};

static Completions complete_face(const Context& context, CompletionFlags flags,
                                 StringView prefix, ByteCount cursor_pos)
{
    return {0_byte, cursor_pos,
            complete(prefix, cursor_pos, context.faces().flatten_faces() |
                     transform([](auto& entry) -> const String& { return entry.key; }))};
}

static String face_doc_helper(const Context& context, CommandParameters params)
{
    if (params.size() < 2)
        return {};
    try
    {
        auto face = context.faces()[params[1]];
        return format("{}:\n{}", params[1], indent(to_string(face)));
    }
    catch (runtime_error&)
    {
        return {};
    }
}

const CommandDesc set_face_cmd = {
    "set-face",
    "face",
    "set-face <scope> <name> <facespec>: set face <name> to <facespec> in <scope>\n"
    "\n"
    "facespec format is:\n"
    "    <fg color>[,<bg color>[,<underline color>]][+<attributes>][@<base>]\n"
    "colors are either a color name, rgb:######, or rgba:######## values.\n"
    "attributes is a combination of:\n"
    "    u: underline, c: curly underline, i: italic, b: bold,\n"
    "    r: reverse,   s: strikethrough,   B: blink,  d: dim,\n"
    "    f: final foreground,              g: final background,\n"
    "    a: final attributes,              F: same as +fga\n"
    "facespec can as well just be the name of another face.\n"
    "if a base face is specified, colors and attributes are applied on top of it",
    ParameterDesc{{}, ParameterDesc::Flags::None, 3, 3},
    CommandFlags::None,
    face_doc_helper,
    make_completer(menu(complete_scope), complete_face, complete_face),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        get_scope(parser[0], context).faces().add_face(parser[1], parser[2], true);

        for (auto& client : ClientManager::instance())
            client->force_redraw();
    }
};

const CommandDesc unset_face_cmd = {
    "unset-face",
    nullptr,
    "unset-face <scope> <name>: remove <face> from <scope>",
    double_params,
    CommandFlags::None,
    face_doc_helper,
    make_completer(menu(complete_scope), menu(complete_face)),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
       get_scope(parser[0], context).faces().remove_face(parser[1]);
    }
};

const CommandDesc rename_client_cmd = {
    "rename-client",
    nullptr,
    "rename-client <name>: set current client name to <name>",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    make_single_word_completer([](const Context& context){ return context.name(); }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        const String& name = parser[0];
        if (not all_of(name, is_identifier))
            throw runtime_error{format("invalid client name: '{}'", name)};
        else if (ClientManager::instance().client_name_exists(name) and
                 context.name() != name)
            throw runtime_error{format("client name '{}' is not unique", name)};
        else
            context.set_name(name);
    }
};

const CommandDesc set_register_cmd = {
    "set-register",
    "reg",
    "set-register <name> <values>...: set register <name> to <values>",
    ParameterDesc{{}, ParameterDesc::Flags::SwitchesAsPositional, 1},
    CommandFlags::None,
    CommandHelper{},
    make_completer(
         [](const Context& context, CompletionFlags flags,
            StringView prefix, ByteCount cursor_pos) -> Completions {
             return { 0_byte, cursor_pos,
                      RegisterManager::instance().complete_register_name(prefix, cursor_pos) };
        }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        RegisterManager::instance()[parser[0]].set(context, parser.positionals_from(1));
    }
};

const CommandDesc select_cmd = {
    "select",
    nullptr,
    "select <selection_desc>...: select given selections\n"
    "\n"
    "selection_desc format is <anchor_line>.<anchor_column>,<cursor_line>.<cursor_column>",
    ParameterDesc{{
            {"timestamp", {true, "specify buffer timestamp at which those selections are valid"}},
            {"codepoint", {false, "columns are specified in codepoints, not bytes"}},
            {"display-column", {false, "columns are specified in display columns, not bytes"}}
        },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 1
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        auto& buffer = context.buffer();
        const size_t timestamp = parser.get_switch("timestamp").map(str_to_int_ifp).cast<size_t>().value_or(buffer.timestamp());
        ColumnType column_type = ColumnType::Byte;
        if (parser.get_switch("codepoint"))
            column_type = ColumnType::Codepoint;
        else if (parser.get_switch("display-column"))
            column_type = ColumnType::DisplayColumn;
        ColumnCount tabstop = context.options()["tabstop"].get<int>();
        context.selections_write_only() = selection_list_from_strings(buffer, column_type, parser.positionals_from(0), timestamp, 0, tabstop);
    }
};

const CommandDesc change_directory_cmd = {
    "change-directory",
    "cd",
    "change-directory [<directory>]: change the server's working directory to <directory>, or the home directory if unspecified",
    single_optional_param,
    CommandFlags::None,
    CommandHelper{},
    make_completer(
         [](const Context& context, CompletionFlags flags,
            StringView prefix, ByteCount cursor_pos) -> Completions {
             return { 0_byte, cursor_pos,
                      complete_filename(prefix,
                                        context.options()["ignored_files"].get<Regex>(),
                                        cursor_pos, FilenameFlags::OnlyDirectories),
                      Completions::Flags::Menu };
        }),
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        StringView target = parser.positional_count() == 1 ? StringView{parser[0]} : "~";
        if (chdir(parse_filename(target).c_str()) != 0)
            throw runtime_error(format("unable to change to directory: '{}'", target));
        for (auto& buffer : BufferManager::instance())
            buffer->update_display_name();
    }
};

const CommandDesc rename_session_cmd = {
    "rename-session",
    nullptr,
    "rename-session <name>: change remote session name",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    make_single_word_completer([](const Context&){ return Server::instance().session(); }),
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        if (not Server::instance().rename_session(parser[0]))
            throw runtime_error(format("unable to rename current session: '{}' may be already in use", parser[0]));
    }
};

const CommandDesc fail_cmd = {
    "fail",
    nullptr,
    "fail [<message>]: raise an error with the given message",
    ParameterDesc{},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        throw failure{join(parser, " ")};
    }
};

const CommandDesc declare_user_mode_cmd = {
    "declare-user-mode",
    nullptr,
    "declare-user-mode <name>: add a new user keymap mode",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        context.keymaps().add_user_mode(std::move(parser[0]));
    }
};

// We need ownership of the mode_name in the lock case
void enter_user_mode(Context& context, String mode_name, KeymapMode mode, bool lock)
{
    on_next_key_with_autoinfo(context, format("user.{}", mode_name), KeymapMode::None,
                             [mode_name, mode, lock](Key key, Context& context) mutable {
        if (key == Key::Escape)
            return;
        if (not context.keymaps().is_mapped(key, mode))
            return;

        auto& mapping = context.keymaps().get_mapping(key, mode);
        ScopedSetBool disable_keymaps(context.keymaps_disabled());
        ScopedSetBool disable_history(context.history_disabled());

        InputHandler::ScopedForceNormal force_normal{context.input_handler(), {}};

        ScopedEdition edition(context);
        for (auto& key : mapping.keys)
            context.input_handler().handle_key(key);

        if (lock)
            enter_user_mode(context, std::move(mode_name), mode, true);
    }, lock ? format("{} (lock)", mode_name) : mode_name,
    build_autoinfo_for_mapping(context, mode, {}));
}

const CommandDesc enter_user_mode_cmd = {
    "enter-user-mode",
    nullptr,
    "enter-user-mode [<switches>] <name>: enable <name> keymap mode for next key",
    ParameterDesc{
        { { "lock", { false, "stay in mode until <esc> is pressed" } } },
        ParameterDesc::Flags::None, 1, 1
    },
    CommandFlags::None,
    CommandHelper{},
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
        {
            return { 0_byte, params[0].length(),
                     complete(params[0], pos_in_token, context.keymaps().user_modes()),
                     Completions::Flags::Menu };
        }
        return {};
    },
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        auto lock = (bool)parser.get_switch("lock");
        KeymapMode mode = parse_keymap_mode(parser[0], context.keymaps().user_modes());
        enter_user_mode(context, std::move(parser[0]), mode, lock);
    }
};

const CommandDesc provide_module_cmd = {
    "provide-module",
    nullptr,
    "provide-module [<switches>] <name> <cmds>: declares a module <name> provided by <cmds>",
    ParameterDesc{
        { { "override", { false, "allow overriding an existing module" } } },
        ParameterDesc::Flags::None,
        2, 2
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        const String& module_name = parser[0];
        auto& cm = CommandManager::instance();

        if (not all_of(module_name, is_identifier))
            throw runtime_error(format("invalid module name: '{}'", module_name));

        if (cm.module_defined(module_name) and not parser.get_switch("override"))
            throw runtime_error(format("module '{}' already defined", module_name));
        cm.register_module(module_name, parser[1]);
    }
};

const CommandDesc require_module_cmd = {
    "require-module",
    nullptr,
    "require-module <name>: ensures that <name> module has been loaded",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    make_completer(menu(
         [](const Context&, CompletionFlags, StringView prefix, ByteCount cursor_pos) {
            return CommandManager::instance().complete_module_name(prefix.substr(0, cursor_pos));
        })),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        CommandManager::instance().load_module(parser[0], context);
    }
};

}

void register_commands()
{
    CommandManager& cm = CommandManager::instance();
    cm.register_command("nop", [](const ParametersParser&, Context&, const ShellContext&){}, "do nothing",
        {{}, ParameterDesc::Flags::IgnoreUnknownSwitches});

    auto register_command = [&](const CommandDesc& c)
    {
        cm.register_command(c.name, c.func, c.docstring, c.params, c.flags, c.helper, c.completer);
        if (c.alias)
            GlobalScope::instance().aliases().add_alias(c.alias, c.name);
    };

    register_command(edit_cmd);
    register_command(force_edit_cmd);
    register_command(write_cmd);
    register_command(force_write_cmd);
    register_command(write_all_cmd);
    register_command(write_all_quit_cmd);
    register_command(kill_cmd);
    register_command(force_kill_cmd);
    register_command(quit_cmd);
    register_command(force_quit_cmd);
    register_command(write_quit_cmd);
    register_command(force_write_quit_cmd);
    register_command(buffer_cmd);
    register_command(buffer_next_cmd);
    register_command(buffer_previous_cmd);
    register_command(delete_buffer_cmd);
    register_command(force_delete_buffer_cmd);
    register_command(rename_buffer_cmd);
    register_command(arrange_buffers_cmd);
    register_command(add_highlighter_cmd);
    register_command(remove_highlighter_cmd);
    register_command(add_hook_cmd);
    register_command(remove_hook_cmd);
    register_command(trigger_user_hook_cmd);
    register_command(define_command_cmd);
    register_command(complete_command_cmd);
    register_command(alias_cmd);
    register_command(unalias_cmd);
    register_command(echo_cmd);
    register_command(debug_cmd);
    register_command(source_cmd);
    register_command(set_option_cmd);
    register_command(unset_option_cmd);
    register_command(update_option_cmd);
    register_command(declare_option_cmd);
    register_command(map_key_cmd);
    register_command(unmap_key_cmd);
    register_command(execute_keys_cmd);
    register_command(evaluate_commands_cmd);
    register_command(prompt_cmd);
    register_command(menu_cmd);
    register_command(on_key_cmd);
    register_command(info_cmd);
    register_command(try_catch_cmd);
    register_command(set_face_cmd);
    register_command(unset_face_cmd);
    register_command(rename_client_cmd);
    register_command(set_register_cmd);
    register_command(select_cmd);
    register_command(change_directory_cmd);
    register_command(rename_session_cmd);
    register_command(fail_cmd);
    register_command(declare_user_mode_cmd);
    register_command(enter_user_mode_cmd);
    register_command(provide_module_cmd);
    register_command(require_module_cmd);
}

}
