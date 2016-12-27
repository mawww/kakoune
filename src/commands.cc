#include "commands.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "completion.hh"
#include "containers.hh"
#include "context.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "highlighter.hh"
#include "highlighters.hh"
#include "option_manager.hh"
#include "option_types.hh"
#include "parameters_parser.hh"
#include "ranked_match.hh"
#include "register_manager.hh"
#include "insert_completer.hh"
#include "remote.hh"
#include "regex.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "user_interface.hh"
#include "window.hh"

#include <functional>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__GLIBC__) || defined(__CYGWIN__)
#include <malloc.h>
#endif

namespace Kakoune
{

template<>
struct option_type_name<TimestampedList<LineAndFlag>>
{
    static StringView name() { return "line-flags"; }
};

template<>
struct option_type_name<TimestampedList<RangeAndFace>>
{
    static StringView name() { return "range-faces"; }
};

namespace
{

Buffer* open_fifo(StringView name, StringView filename, bool scroll)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (fd < 0)
       throw runtime_error(format("unable to open '{}'", filename));

    return create_fifo_buffer(name.str(), fd, scroll);
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
    template<typename C, typename... R,
             typename = typename std::enable_if<not std::is_base_of<PerArgumentCommandCompleter<>,
                                                typename std::remove_reference<C>::type>::value>::type>
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
PerArgumentCommandCompleter<typename std::decay<Completers>::type...>
make_completer(Completers&&... completers)
{
    return {std::forward<Completers>(completers)...};
}

auto filename_completer = make_completer(
    [](const Context& context, CompletionFlags flags, const String& prefix, ByteCount cursor_pos)
    { return Completions{ 0_byte, cursor_pos,
                          complete_filename(prefix,
                                            context.options()["ignored_files"].get<Regex>(),
                                            cursor_pos, FilenameFlags::Expand) }; });

static Completions complete_buffer_name(const Context& context, CompletionFlags flags,
                                        StringView prefix, ByteCount cursor_pos)
{
    struct RankedMatchAndBuffer : RankedMatch
    {
        RankedMatchAndBuffer(const RankedMatch& m, const Buffer* b)
            : RankedMatch{m}, buffer{b} {}

        using RankedMatch::operator==;
        using RankedMatch::operator<;

        const Buffer* buffer;
    };

    StringView query = prefix.substr(0, cursor_pos);
    Vector<RankedMatchAndBuffer> filename_matches;
    Vector<RankedMatchAndBuffer> matches;
    for (const auto& buffer : BufferManager::instance())
    {
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

auto buffer_completer = make_completer(complete_buffer_name);

const ParameterDesc no_params{ {}, ParameterDesc::Flags::None, 0, 0 };
const ParameterDesc single_name_param{ {}, ParameterDesc::Flags::None, 1, 1 };
const ParameterDesc single_optional_name_param{ {}, ParameterDesc::Flags::None, 0, 1 };

static constexpr auto scopes = { "global", "buffer", "window" };

static Completions complete_scope(const Context&, CompletionFlags,
                                  const String& prefix, ByteCount cursor_pos)
{
   return { 0_byte, cursor_pos, complete(prefix, cursor_pos, scopes) };
}


static Completions complete_command_name(const Context& context, CompletionFlags,
                                         const String& prefix, ByteCount cursor_pos)
{
   return CommandManager::instance().complete_command_name(
       context, prefix.substr(0, cursor_pos), false);
}


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
    throw runtime_error(format("error: no such scope '{}'", scope));
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
    if (parser.positional_count() == 0 and not force_reload)
        throw wrong_argument_count();

    auto& name = parser.positional_count() > 0 ? parser[0]
                                               : context.buffer().name();
    auto& buffer_manager = BufferManager::instance();

    Buffer* buffer = buffer_manager.get_buffer_ifp(name);
    const bool no_hooks = context.hooks_disabled();
    const auto flags = no_hooks ? Buffer::Flags::NoHooks : Buffer::Flags::None;

    if (force_reload and buffer and buffer->flags() & Buffer::Flags::File)
        reload_file_buffer(*buffer);
    else
    {
        if (parser.get_switch("scratch"))
        {
            if (buffer and (force_reload or buffer->flags() != Buffer::Flags::None))
            {
                buffer_manager.delete_buffer(*buffer);
                buffer = nullptr;
            }

            if (not buffer)
                buffer = buffer_manager.create_buffer(name, flags);
        }
        else if (auto fifo = parser.get_switch("fifo"))
            buffer = open_fifo(name, *fifo, (bool)parser.get_switch("scroll"));
        else if (not buffer)
        {
            buffer = parser.get_switch("existing") ? open_file_buffer(name, flags)
                                                   : open_or_create_file_buffer(name, flags);
            if (buffer->flags() & Buffer::Flags::New)
                context.print_status({ format("new file '{}'", name),
                                       get_face("StatusLine") });
        }

        buffer->flags() &= ~Buffer::Flags::NoHooks;
    }

    const size_t param_count = parser.positional_count();
    if (buffer != &context.buffer() or param_count > 1)
        context.push_jump();

    if (buffer != &context.buffer())
        context.change_buffer(*buffer);

    if (param_count > 1 and not parser[1].empty())
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
    { { "existing", { false, "fail if the file does not exists, do not open a new file" } },
      { "scratch",  { false, "create a scratch buffer, not linked to a file" } },
      { "fifo",     { true,  "create a buffer reading its content from a named fifo" } },
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
    filename_completer,
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
    filename_completer,
    edit<true>
};

void write_buffer(const ParametersParser& parser, Context& context, const ShellContext&)
{
    Buffer& buffer = context.buffer();

    if (parser.positional_count() == 0 and !(buffer.flags() & Buffer::Flags::File))
        throw runtime_error("cannot write a non file buffer without a filename");

    // if the buffer is in read-only mode and we try to save it directly
    // or we try to write to it indirectly using e.g. a symlink, throw an error
    if ((context.buffer().flags() & Buffer::Flags::ReadOnly)
        and (parser.positional_count() == 0 or real_path(parser[0]) == buffer.name()))
        throw runtime_error("cannot overwrite the buffer when in readonly mode");

    auto filename = parser.positional_count() == 0 ?
                        buffer.name() : parse_filename(parser[0]);

    context.hooks().run_hook("BufWritePre", filename, context);
    write_buffer_to_file(buffer, filename);
    context.hooks().run_hook("BufWritePost", filename, context);
}

const CommandDesc write_cmd = {
    "write",
    "w",
    "write [filename]: write the current buffer to its file "
    "or to [filename] if specified",
    single_optional_name_param,
    CommandFlags::None,
    CommandHelper{},
    filename_completer,
    write_buffer,
};

void write_all_buffers(Context& context)
{
    // Copy buffer list because hooks might be creating/deleting buffers
    Vector<SafePtr<Buffer>> buffers;
    for (auto& buffer : BufferManager::instance())
        buffers.emplace_back(buffer.get());

    for (auto& buffer : buffers)
    {
        if ((buffer->flags() & Buffer::Flags::File) and buffer->is_modified()
            and !(buffer->flags() & Buffer::Flags::ReadOnly))
        {
            buffer->run_hook_in_own_context("BufWritePre", buffer->name(), context.name());
            write_buffer_to_file(*buffer, buffer->name());
            buffer->run_hook_in_own_context("BufWritePost", buffer->name(), context.name());
        }
    }
}

const CommandDesc write_all_cmd = {
    "write-all",
    "wa",
    "write all buffers that are associated to a file",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser&, Context& context, const ShellContext&){ write_all_buffers(context); }
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

    String message = "modified buffers remaining: [";
    while (it != end)
    {
        message += (*it)->name();
        it = std::find_if(it+1, end, is_modified);
        message += (it != end) ? ", " : "]";
    }
    throw runtime_error(message);
}

const CommandDesc kill_cmd = {
    "kill",
    nullptr,
    "kill current session, quit all clients and server",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser&, Context&, const ShellContext&){
        ensure_all_buffers_are_saved();
        throw kill_session{};
    }
};


const CommandDesc force_kill_cmd = {
    "kill!",
    nullptr,
    "kill current session, quit all clients and server, do not check for unsaved buffers",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser&, Context&, const ShellContext&){ throw kill_session{}; }
};

template<bool force>
void quit(Context& context)
{
    if (not force and ClientManager::instance().count() == 1)
        ensure_all_buffers_are_saved();

    ClientManager::instance().remove_client(context.client(), true);
}

const CommandDesc quit_cmd = {
    "quit",
    "q",
    "quit current client, and the kakoune session if the client is the last "
    "(if not running in daemon mode)",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser&, Context& context, const ShellContext&){ quit<false>(context); }
};

const CommandDesc force_quit_cmd = {
    "quit!",
    "q!",
    "quit current client, and the kakoune session if the client is the last "
    "(if not running in daemon mode). force quit even if the client is the "
    "last and some buffers are not saved.",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser&, Context& context, const ShellContext&){ quit<true>(context); }
};

template<bool force>
void write_quit(const ParametersParser& parser, Context& context,
                const ShellContext& shell_context)
{
    write_buffer(parser, context, shell_context);
    quit<force>(context);
}

const CommandDesc write_quit_cmd = {
    "write-quit",
    "wq",
    "write current buffer and quit current client",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    write_quit<false>
};

const CommandDesc force_write_quit_cmd = {
    "write-quit!",
    "wq!",
    "write current buffer and quit current client, even if other buffers are "
    "not saved",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    write_quit<true>
};

const CommandDesc write_all_quit_cmd = {
    "write-all-quit",
    "waq",
    "write all buffers associated to a file and quit current client",
    no_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        write_all_buffers(context);
        quit<false>(context);
    }
};

const CommandDesc buffer_cmd = {
    "buffer",
    "b",
    "buffer <name>: set buffer to edit in current client",
    single_name_param,
    CommandFlags::None,
    CommandHelper{},
    buffer_completer,
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
    if (newbuf->flags() & Buffer::Flags::Debug)
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
}

const CommandDesc delete_buffer_cmd = {
    "delete-buffer",
    "db",
    "delete-buffer [name]: delete current buffer or the buffer named <name> if given",
    single_optional_name_param,
    CommandFlags::None,
    CommandHelper{},
    buffer_completer,
    delete_buffer<false>
};

const CommandDesc force_delete_buffer_cmd = {
    "delete-buffer!",
    "db!",
    "delete-buffer! [name]: delete current buffer or the buffer named <name> if "
    "given, even if the buffer is unsaved",
    single_optional_name_param,
    CommandFlags::None,
    CommandHelper{},
    buffer_completer,
    delete_buffer<true>
};

const CommandDesc rename_buffer_cmd = {
    "rename-buffer",
    nullptr,
    "rename-buffer <name>: change current buffer name",
    single_name_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not context.buffer().set_name(parser[0]))
            throw runtime_error(format("unable to change buffer name to '{}'", parser[0]));
    }
};

Completions complete_highlighter(const Context& context,
                                 StringView arg, ByteCount pos_in_token, bool only_group)
{
    const bool shared = not arg.empty() and arg[0_byte] == '/';
    if (shared)
    {
        auto& group = DefinedHighlighters::instance();
        return offset_pos(group.complete_child(arg.substr(1_byte), pos_in_token-1, only_group), 1);
    }
    else
    {
        auto& group = context.window().highlighters();
        return group.complete_child(arg, pos_in_token, only_group);
    }
}

Completions remove_highlighter_completer(
    const Context& context, CompletionFlags flags, CommandParameters params,
    size_t token_to_complete, ByteCount pos_in_token)
{
    const String& arg = params[token_to_complete];
    if (token_to_complete == 0 and not arg.empty() and arg.front() == '/')
    {
        auto& group = DefinedHighlighters::instance();
        return offset_pos(group.complete_child(arg.substr(1_byte), pos_in_token-1, false), 1);
    }
    else if (token_to_complete == 0)
        return context.window().highlighters().complete_child(arg, pos_in_token, false);
    return {};
}

Completions add_highlighter_completer(
    const Context& context, CompletionFlags flags, CommandParameters params,
    size_t token_to_complete, ByteCount pos_in_token)
{
    StringView arg = params[token_to_complete];
    if (token_to_complete == 1 and params[0] == "-group")
        return complete_highlighter(context, params[1], pos_in_token, true);
    else if (token_to_complete == 0 or (token_to_complete == 2 and params[0] == "-group"))
        return { 0_byte, arg.length(), complete(arg, pos_in_token, HighlighterRegistry::instance() | transform(HighlighterRegistry::get_id)) };
    return Completions{};
}

Highlighter& get_highlighter(const Context& context, StringView path)
{
    if (path.empty())
        throw runtime_error("group path should not be empty");

    Highlighter* root = nullptr;
    if (path[0_byte] == '/')
    {
        root = &DefinedHighlighters::instance();
        path = path.substr(1_byte);
    }
    else
        root = &context.window().highlighters();

    if (path.back() == '/')
        path = path.substr(0_byte, path.length() - 1);

    if (not path.empty())
        return root->get_child(path);
    return *root;
}

const CommandDesc add_highlighter_cmd = {
    "add-highlighter",
    "addhl",
    "add-highlighter <type> <type params>...: add an highlighter",
    ParameterDesc{
        { { "group", { true, "Set the group in which to put the highlighter. "
                             "If starting with /, search in shared highlighters, "
                             "else search in the current window" } } },
      ParameterDesc::Flags::SwitchesOnlyAtStart, 1
    },
    CommandFlags::None,
    [](const Context& context, CommandParameters params) -> String
    {
        if (params.size() > 0)
        {
            HighlighterRegistry& registry = HighlighterRegistry::instance();
            auto it = registry.find(params[0]);
            if (it != registry.end())
                return format("{}:\n{}", params[0], indent(it->value.docstring));
        }
        return "";
    },
    add_highlighter_completer,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        auto begin = parser.begin();
        const String& name = *begin++;
        Vector<String> highlighter_params;
        for (; begin != parser.end(); ++begin)
            highlighter_params.push_back(*begin);

        auto group_name = parser.get_switch("group");
        auto& group = group_name ? get_highlighter(context, *group_name)
                                 : context.window().highlighters();
        auto it = registry.find(name);
        if (it == registry.end())
            throw runtime_error(format("No such highlighter factory '{}'", name));
        group.add_child(it->value.factory(highlighter_params));

        if (context.has_window())
            context.window().force_redraw();
    }
};

const CommandDesc remove_highlighter_cmd = {
    "remove-highlighter",
    "rmhl",
    "add-highlighter <path>: remove highlighter <name>",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 1, 1 },
    CommandFlags::None,
    CommandHelper{},
    remove_highlighter_completer,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        StringView path = parser[0];
        auto sep_it = find(path | reverse(), '/');
        auto& group = sep_it != path.rend() ?
            get_highlighter(context, {path.begin(), sep_it.base()})
          : context.window().highlighters();

        group.remove_child({sep_it.base(), path.end()});

        if (context.has_window())
            context.window().force_redraw();
    }
};

static constexpr auto hooks = {
    "BufCreate", "BufNew", "BufOpen", "BufClose", "BufWritePost", "BufWritePre",
    "BufOpenFifo", "BufCloseFifo", "BufReadFifo", "BufSetOption",
    "InsertBegin", "InsertChar", "InsertEnd", "InsertIdle", "InsertKey",
    "InsertMove", "InsertCompletionHide", "InsertCompletionShow",
    "KakBegin", "KakEnd", "FocusIn", "FocusOut", "RuntimeError",
    "NormalBegin", "NormalEnd", "NormalIdle", "NormalKey",
    "WinClose", "WinCreate", "WinDisplay", "WinResize", "WinSetOption",
};

static Completions complete_hooks(const Context&, CompletionFlags,
                                  const String& prefix, ByteCount cursor_pos)
{
    return { 0_byte, cursor_pos, complete(prefix, cursor_pos, hooks) };
}

const CommandDesc add_hook_cmd = {
    "hook",
    nullptr,
    "hook <switches> <scope> <hook_name> <filter> <command>: add <command> in <scope> "
    "to be executed on hook <hook_name> when its parameter matches the <filter> regex\n"
    "scope can be: \n"
    "  * global: hook is executed for any buffer or window\n"
    "  * buffer: hook is executed only for the current buffer\n"
    "            (and any window for that buffer)\n"
    "  * window: hook is executed only for the current window\n",
    ParameterDesc{
        { { "group", { true, "set hook group, see rmhooks" } } },
        ParameterDesc::Flags::None, 4, 4
    },
    CommandFlags::None,
    CommandHelper{},
    make_completer(complete_scope, complete_hooks, complete_nothing,
                   [](const Context& context, CompletionFlags flags,
                      const String& prefix, ByteCount cursor_pos)
                   { return CommandManager::instance().complete(
                         context, flags, prefix, cursor_pos); }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not contains(hooks, parser[1]))
            throw runtime_error{format("Unknown hook '{}'", parser[1])};

        Regex regex(parser[2], Regex::optimize | Regex::nosubs | Regex::ECMAScript);
        const String& command = parser[3];

        auto hook_func = [=](StringView param, Context& context) {
            ScopedSetBool disable_history{context.history_disabled()};

            if (regex_match(param.begin(), param.end(), regex))
                CommandManager::instance().execute(command, context,
                                                   { {}, { { "hook_param", param.str() } } });
        };
        auto group = parser.get_switch("group").value_or(StringView{});
        get_scope(parser[0], context).hooks().add_hook(parser[1], group.str(), hook_func);
    }
};

const CommandDesc remove_hook_cmd = {
    "remove-hooks",
    "rmhooks",
    "remove-hooks <scope> <group>: remove all hooks whose group is <group>",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 2, 2 },
    CommandFlags::None,
    CommandHelper{},
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return { 0_byte, params[0].length(),
                     complete(params[0], pos_in_token, scopes) };
        else if (token_to_complete == 1)
        {
            if (auto scope = get_scope_ifp(params[0], context))
                return { 0_byte, params[0].length(),
                         scope->hooks().complete_hook_group(params[1], pos_in_token) };
        }
        return {};
    },
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        get_scope(parser[0], context).hooks().remove_hooks(parser[1]);
    }
};

Vector<String> params_to_shell(const ParametersParser& parser)
{
    Vector<String> vars;
    for (size_t i = 0; i < parser.positional_count(); ++i)
        vars.push_back(parser[i]);
    return vars;
}

void define_command(const ParametersParser& parser, Context& context, const ShellContext&)
{
    const String& cmd_name = parser[0];
    auto& cm = CommandManager::instance();

    if (cm.command_defined(cmd_name) and not parser.get_switch("allow-override"))
        throw runtime_error(format("command '{}' already defined", cmd_name));

    CommandFlags flags = CommandFlags::None;
    if (parser.get_switch("hidden"))
        flags = CommandFlags::Hidden;

    const String& commands = parser[1];
    Command cmd;
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
        cmd = [=](const ParametersParser& parser, Context& context, const ShellContext&) {
            CommandManager::instance().execute(commands, context, { params_to_shell(parser) });
        };
    }
    else
    {
        desc = ParameterDesc{ {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 0 };
        cmd = [=](const ParametersParser& parser, Context& context, const ShellContext&) {
            CommandManager::instance().execute(commands, context);
        };
    }

    CommandCompleter completer;
    if (parser.get_switch("file-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = params[token_to_complete];
             auto& ignored_files = context.options()["ignored_files"].get<Regex>();
             return Completions{ 0_byte, pos_in_token,
                                 complete_filename(prefix, ignored_files,
                                                   pos_in_token, FilenameFlags::Expand) };
        };
    }
    else if (parser.get_switch("client-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             const String& prefix = params[token_to_complete];
             auto& cm = ClientManager::instance();
             return Completions{ 0_byte, pos_in_token,
                                 cm.complete_client_name(prefix, pos_in_token) };
        };
    }
    else if (parser.get_switch("buffer-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
             return complete_buffer_name(context, flags, params[token_to_complete], pos_in_token);
        };
    }
    else if (auto shell_cmd_opt = parser.get_switch("shell-completion"))
    {
        String shell_cmd = shell_cmd_opt->str();
        completer = [=](const Context& context, CompletionFlags flags,
                        CommandParameters params,
                        size_t token_to_complete, ByteCount pos_in_token)
        {
            if (flags & CompletionFlags::Fast) // no shell on fast completion
                return Completions{};

            ShellContext shell_context{
                params,
                { { "token_to_complete", to_string(token_to_complete) },
                  { "pos_in_token",      to_string(pos_in_token) } }
            };
            String output = ShellManager::instance().eval(shell_cmd, context, {},
                                                          ShellManager::Flags::WaitForStdout,
                                                          shell_context).first;
            CandidateList candidates;
            for (auto& str : split(output, '\n', 0))
                candidates.push_back(std::move(str));

            return Completions{ 0_byte, pos_in_token, std::move(candidates) };
        };
    }
    else if (auto shell_cmd_opt = parser.get_switch("shell-candidates"))
    {
        String shell_cmd = shell_cmd_opt->str();
        Vector<std::pair<String, UsedLetters>> candidates;
        int token = -1;
        completer = [shell_cmd, candidates, token](
            const Context& context, CompletionFlags flags, CommandParameters params,
            size_t token_to_complete, ByteCount pos_in_token) mutable
        {
            if (flags & CompletionFlags::Start)
                token = -1;

            if (token != token_to_complete)
            {
                ShellContext shell_context{
                    params,
                    { { "token_to_complete", to_string(token_to_complete) } }
                };
                String output = ShellManager::instance().eval(shell_cmd, context, {},
                                                              ShellManager::Flags::WaitForStdout,
                                                              shell_context).first;
                candidates.clear();
                for (auto c : output | split<StringView>('\n'))
                    candidates.push_back({c.str(), used_letters(c)});
                token = token_to_complete;
            }

            StringView query = params[token_to_complete].substr(0, pos_in_token);
            UsedLetters query_letters = used_letters(query);
            Vector<RankedMatch> matches;
            for (const auto& candidate : candidates)
            {
                if (RankedMatch match{candidate.first, candidate.second, query, query_letters})
                    matches.push_back(match);
            }

            constexpr size_t max_count = 100;
            // Gather best max_count matches
            auto greater = [](const RankedMatch& lhs,
                              const RankedMatch& rhs) { return rhs < lhs; };
            auto first = matches.begin(), last = matches.end();
            std::make_heap(first, last, greater);
            CandidateList res;
            while (res.size() < max_count and first != last)
            {
                if (res.empty() or res.back() != first->candidate())
                    res.push_back(first->candidate().str());
                std::pop_heap(first, last--, greater);
            }

            return Completions{ 0_byte, pos_in_token, std::move(res) };
        };
    }
    else if (parser.get_switch("command-completion"))
    {
        completer = [](const Context& context, CompletionFlags flags,
                       CommandParameters params,
                       size_t token_to_complete, ByteCount pos_in_token)
        {
            return CommandManager::instance().complete(
                context, flags, params, token_to_complete, pos_in_token);
        };
    }

    auto docstring = parser.get_switch("docstring").value_or(StringView{});

    cm.register_command(cmd_name, cmd, docstring.str(), desc, flags, CommandHelper{}, completer);
}

const CommandDesc define_command_cmd = {
    "define-command",
    "def",
    "define-command <switches> <name> <cmds>: define a command <name> executing <cmds>",
    ParameterDesc{
        { { "params",             { true, "take parameters, accessible to each shell escape as $0..$N\n"
                                          "parameter should take the form <count> or <min>..<max> (both omittable)" } },
          { "allow-override",     { false, "allow overriding an existing command" } },
          { "hidden",             { false, "do not display the command in completion candidates" } },
          { "docstring",          { true,  "define the documentation string for command" } },
          { "file-completion",    { false, "complete parameters using filename completion" } },
          { "client-completion",  { false, "complete parameters using client name completion" } },
          { "buffer-completion",  { false, "complete parameters using buffer name completion" } },
          { "command-completion", { false, "complete parameters using kakoune command completion" } },
          { "shell-completion",   { true,  "complete the parameters using the given shell-script" } },
          { "shell-candidates",   { true,  "get the parameter candidates using the given shell-script" } } },
        ParameterDesc::Flags::None,
        2, 2
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    define_command
};

const CommandDesc alias_cmd = {
    "alias",
    nullptr,
    "alias <scope> <alias> <command>: alias <alias> to <command> in <scope>\n",
    ParameterDesc{{}, ParameterDesc::Flags::None, 3, 3},
    CommandFlags::None,
    CommandHelper{},
    make_completer(complete_scope, complete_nothing, complete_command_name),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not CommandManager::instance().command_defined(parser[2]))
            throw runtime_error(format("Command '{}' does not exist", parser[2]));

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
    make_completer(complete_scope, complete_nothing, complete_command_name),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        AliasRegistry& aliases = get_scope(parser[0], context).aliases();
        if (parser.positional_count() == 3 and
            aliases[parser[1]] != parser[2])
            return;
        aliases.remove_alias(parser[1]);
    }
};

const CommandDesc echo_cmd = {
    "echo",
    nullptr,
    "echo <params>...: display given parameters in the status line",
    ParameterDesc{
        { { "color", { true,  "set message color" } },
          { "markup", { false, "parse markup" } },
          { "debug", { false, "write to debug buffer instead of status line" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        String message = join(parser, ' ', false);
        if (parser.get_switch("debug"))
            write_to_debug_buffer(message);
        else if (parser.get_switch("markup"))
            context.print_status(parse_display_line(message));
        else
        {
            auto face = get_face(parser.get_switch("color").value_or("StatusLine").str());
            context.print_status({ std::move(message), face } );
        }
    }
};


const CommandDesc debug_cmd = {
    "debug",
    nullptr,
    "debug <command>: write some debug informations in the debug buffer\n"
    "existing commands: info, buffers, options, memory, shared-strings",
    ParameterDesc{{}, ParameterDesc::Flags::SwitchesOnlyAtStart, 1},
    CommandFlags::None,
    CommandHelper{},
    make_completer(
        [](const Context& context, CompletionFlags flags,
           const String& prefix, ByteCount cursor_pos) -> Completions {
               auto c = {"info", "buffers", "options", "memory", "shared-strings"};
               return { 0_byte, cursor_pos, complete(prefix, cursor_pos, c) };
    }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (parser[0] == "info")
        {
            write_to_debug_buffer(format("pid: {}", getpid()));
            write_to_debug_buffer(format("session: {}", Server::instance().session()));
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
                write_to_debug_buffer(format(" * {}: {}", option->name(), option->get_as_string()));
        }
        else if (parser[0] == "memory")
        {
            auto total = 0;
            write_to_debug_buffer("Memory usage:");
            for (int domain = 0; domain < (int)MemoryDomain::Count; ++domain)
            {
                size_t count = domain_allocated_bytes[domain];
                total += count;
                write_to_debug_buffer(format("  {}: {}", domain_name((MemoryDomain)domain), count));
            }
            write_to_debug_buffer(format("  Total: {}", total));
            #if defined(__GLIBC__) || defined(__CYGWIN__)
            write_to_debug_buffer(format("  Malloced: {}", mallinfo().uordblks));
            #endif
        }
        else if (parser[0] == "shared-strings")
        {
            StringRegistry::instance().debug_stats();
        }
        else
            throw runtime_error(format("unknown debug command '{}'", parser[0]));
    }
};

const CommandDesc source_cmd = {
    "source",
    nullptr,
    "source <filename>: execute commands contained in <filename>",
    single_name_param,
    CommandFlags::None,
    CommandHelper{},
    filename_completer,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        String file_content = read_file(parse_filename(parser[0]), true);
        try
        {
            CommandManager::instance().execute(file_content, context);
        }
        catch (Kakoune::runtime_error& err)
        {
            write_to_debug_buffer(format("{}:{}", parser[0], err.what()));
            throw;
        }
    }
};

static String option_doc_helper(const Context& context, CommandParameters params)
{
    if (params.size() < 2)
        return "";

    auto desc = GlobalScope::instance().option_registry().option_desc(params[1]);
    if (not desc or desc->docstring().empty())
        return "";

    return format("{}: {}", desc->name(), desc->docstring());
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
    "set-option <switches> <scope> <name> <value>: set option <name> in <scope> to <value>\n"
    "<scope> can be global, buffer, window, or current which refers to the narrowest\n"
    "scope the option is set in",
    ParameterDesc{
        { { "add", { false, "add to option rather than replacing it" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 3, 3
    },
    CommandFlags::None,
    option_doc_helper,
    [](const Context& context, CompletionFlags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        const bool add = params.size() > 1 and params[0] == "-add";
        const int start = add ? 1 : 0;

        static constexpr auto scopes = { "global", "buffer", "window", "current" };

        if (token_to_complete == start)
            return { 0_byte, params[start].length(),
                     complete(params[start], pos_in_token, scopes) };
        else if (token_to_complete == start + 1)
            return { 0_byte, params[start + 1].length(),
                     GlobalScope::instance().option_registry().complete_option_name(params[start + 1], pos_in_token) };
        else if (not add and token_to_complete == start + 2  and
                 GlobalScope::instance().option_registry().option_exists(params[start + 1]))
        {
            OptionManager& options = get_scope(params[start], context).options();
            String val = options[params[start + 1]].get_as_string();
            if (prefix_match(val, params[start + 2]))
                return { 0_byte, params[start + 2].length(), { std::move(val) } };
        }
        return Completions{};
    },
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        Option& opt = get_options(parser[0], context, parser[1]).get_local_option(parser[1]);
        if (parser.get_switch("add"))
            opt.add_from_string(parser[2]);
        else
            opt.set_from_string(parser[2]);
    }
};

const CommandDesc unset_option_cmd = {
    "unset-option",
    "unset",
    "unset-option <scope> <name>: remove <name> option from scope, falling back on parent scope value"
    "<scope> can be buffer, window, or current which refers to the narrowest\n"
    "scope the option is set in",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 2, 2 },
    CommandFlags::None,
    option_doc_helper,
    [](const Context& context, CompletionFlags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
        {
            static constexpr auto scopes = { "buffer", "window", "current" };
            return { 0_byte, params[0].length(), complete(params[0], pos_in_token, scopes) };
        }
        else if (token_to_complete == 1)
            return { 0_byte, params[1].length(),
                     GlobalScope::instance().option_registry().complete_option_name(params[1], pos_in_token) };
        return Completions{};
    },
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        auto& options = get_options(parser[0], context, parser[1]);
        if (&options == &GlobalScope::instance().options())
            throw runtime_error("Cannot unset options in global scope");
        options.unset_option(parser[1]);
    }
};

const CommandDesc declare_option_cmd = {
    "declare-option",
    "decl",
    "declare-option <type> <name> [value]: declare option <name> of type <type>.\n"
    "set its initial value to <value> if given and the option did not exist\n"
    "Available types:\n"
    "    int: integer\n"
    "    bool: boolean (true/false or yes/no)\n"
    "    str: character string\n"
    "    regex: regular expression\n"
    "    int-list: list of integers\n"
    "    str-list: list of character strings\n"
    "    completions: list of completion candidates"
    "    line-flags: list of line flags\n"
    "    range-faces: list of range faces\n",
    ParameterDesc{
        { { "hidden",    { false, "do not display option name when completing" } },
          { "docstring", { true,  "specify option description" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 2, 3
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        Option* opt = nullptr;

        OptionFlags flags = OptionFlags::None;
        if (parser.get_switch("hidden"))
            flags = OptionFlags::Hidden;

        auto docstring = parser.get_switch("docstring").value_or(StringView{}).str();
        OptionsRegistry& reg = GlobalScope::instance().option_registry();

        if (parser[0] == "int")
            opt = &reg.declare_option<int>(parser[1], docstring, 0, flags);
        else if (parser[0] == "bool")
            opt = &reg.declare_option<bool>(parser[1], docstring, 0, flags);
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
        else if (parser[0] == "line-flags")
            opt = &reg.declare_option<TimestampedList<LineAndFlag>>(parser[1], docstring, {}, flags);
        else if (parser[0] == "range-faces")
            opt = &reg.declare_option<TimestampedList<RangeAndFace>>(parser[1], docstring, {}, flags);
        else
            throw runtime_error(format("unknown type {}", parser[0]));

        if (parser.positional_count() == 3)
            opt->set_from_string(parser[2]);
    }
};

KeymapMode parse_keymap_mode(const String& str)
{
    if (prefix_match("normal", str)) return KeymapMode::Normal;
    if (prefix_match("insert", str)) return KeymapMode::Insert;
    if (prefix_match("menu", str))   return KeymapMode::Menu;
    if (prefix_match("prompt", str)) return KeymapMode::Prompt;
    if (prefix_match("goto", str))   return KeymapMode::Goto;
    if (prefix_match("view", str))   return KeymapMode::View;
    if (prefix_match("user", str))   return KeymapMode::User;
    if (prefix_match("object", str)) return KeymapMode::Object;

    throw runtime_error(format("unknown keymap mode '{}'", str));
}

auto map_key_completer =
    [](const Context& context, CompletionFlags flags,
       CommandParameters params, size_t token_to_complete,
       ByteCount pos_in_token) -> Completions
    {
        if (token_to_complete == 0)
            return { 0_byte, params[0].length(),
                     complete(params[0], pos_in_token, scopes) };
        if (token_to_complete == 1)
        {
            constexpr const char* modes[] = { "normal", "insert", "menu", "prompt", "goto", "view", "user", "object" };
            return { 0_byte, params[1].length(),
                     complete(params[1], pos_in_token, modes) };
        }
        return {};
    };

const CommandDesc map_key_cmd = {
    "map",
    nullptr,
    "map <scope> <mode> <key> <keys>: map <key> to <keys> in given mode in <scope>.\n"
    "<mode> can be:\n"
    "    normal\n"
    "    insert\n"
    "    menu\n"
    "    prompt\n"
    "    goto\n"
    "    view\n"
    "    user\n"
    "    object\n",
    ParameterDesc{{}, ParameterDesc::Flags::None, 4, 4},
    CommandFlags::None,
    CommandHelper{},
    map_key_completer,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        KeymapManager& keymaps = get_scope(parser[0], context).keymaps();
        KeymapMode keymap_mode = parse_keymap_mode(parser[1]);

        KeyList key = parse_keys(parser[2]);
        if (key.size() != 1)
            throw runtime_error("only a single key can be mapped");

        KeyList mapping = parse_keys(parser[3]);
        keymaps.map_key(key[0], keymap_mode, std::move(mapping));
    }
};

const CommandDesc unmap_key_cmd = {
    "unmap",
    nullptr,
    "unmap <scope> <mode> <key> [<expected-keys>]: unmap <key> from given mode in <scope>.\n"
    "If <expected> is specified, remove the mapping only if its value is <expected>\n"
    "<mode> can be:\n"
    "    normal\n"
    "    insert\n"
    "    menu\n"
    "    prompt\n"
    "    goto\n"
    "    view\n"
    "    user\n"
    "    object\n",
    ParameterDesc{{}, ParameterDesc::Flags::None, 3, 4},
    CommandFlags::None,
    CommandHelper{},
    map_key_completer,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        KeymapManager& keymaps = get_scope(parser[0], context).keymaps();
        KeymapMode keymap_mode = parse_keymap_mode(parser[1]);

        KeyList key = parse_keys(parser[2]);
        if (key.size() != 1)
            throw runtime_error("only a single key can be mapped");

        if (keymaps.is_mapped(key[0], keymap_mode) and
            (parser.positional_count() < 4 or
             (keymaps.get_mapping(key[0], keymap_mode) ==
              ConstArrayView<Key>{parse_keys(parser[3])})))
            keymaps.unmap_key(key[0], keymap_mode);
    }
};

const ParameterDesc context_wrap_params = {
    { { "client",     { true,  "run in given client context" } },
      { "try-client", { true,  "run in given client context if it exists, or else in the current one" } },
      { "buffer",     { true,  "run in a disposable context for each given buffer in the comma separated list argument" } },
      { "draft",      { false, "run in a disposable context" } },
      { "no-hooks",   { false, "disable hooks" } },
      { "with-maps",  { false, "use user defined key mapping when executing keys" } },
      { "itersel",    { false, "run once for each selection with that selection as the only one" } },
      { "save-regs",  { true, "restore all given registers after execution (defaults to '/\"|^@')" } },
      { "collapse-jumps",  { false, "collapse all jumps into a single one from initial selection" } } },
    ParameterDesc::Flags::SwitchesOnlyAtStart, 1
};

template<typename T>
struct DisableOption {
    DisableOption(Context& context, const char* name)
        : m_option(context.options()[name]),
          m_prev_value(m_option.get<T>())
    { m_option.set(T{}, false); }

    ~DisableOption() { m_option.set(m_prev_value, false); }

private:
    Option& m_option;
    T m_prev_value;
};

class RegisterRestorer
{
public:
    RegisterRestorer(char name, const Context& context)
      : m_name(name)
    {
        ConstArrayView<String> save = RegisterManager::instance()[name].values(context);
        m_save = Vector<String>(save.begin(), save.end());
    }

    RegisterRestorer(RegisterRestorer&& other) noexcept
        : m_save(std::move(other.m_save)), m_name(other.m_name)
    {
        other.m_name = 0;
    }

    RegisterRestorer& operator=(RegisterRestorer&& other) noexcept
    {
        m_save = std::move(other.m_save);
        m_name = other.m_name;
        other.m_name = 0;
        return *this;
    }

    ~RegisterRestorer()
    {
        if (m_name != 0)
            RegisterManager::instance()[m_name] = m_save;
    }

private:
    Vector<String> m_save;
    char           m_name;
};

template<typename Func>
void context_wrap(const ParametersParser& parser, Context& context, Func func)
{
    // Disable these options to avoid costly code paths (and potential screen
    // redraws) That are useful only in interactive contexts.
    DisableOption<AutoInfo> disable_autoinfo(context, "autoinfo");
    DisableOption<bool> disable_autoshowcompl(context, "autoshowcompl");
    DisableOption<bool> disable_incsearch(context, "incsearch");

    const bool no_hooks = parser.get_switch("no-hooks") or context.hooks_disabled();
    const bool no_keymaps = not parser.get_switch("with-maps");

    Vector<RegisterRestorer> saved_registers;
    for (auto& r : parser.get_switch("save-regs").value_or("/\"|^@"))
        saved_registers.emplace_back(r, context);

    ClientManager& cm = ClientManager::instance();
    if (auto bufnames = parser.get_switch("buffer"))
    {
        auto context_wrap_for_buffer = [&](Buffer& buffer) {
            InputHandler input_handler{{ buffer, Selection{} },
                                       Context::Flags::Transient};
            Context& c = input_handler.context();

            ScopedSetBool disable_hooks(c.hooks_disabled(), no_hooks);
            ScopedSetBool disable_keymaps(c.keymaps_disabled(), no_keymaps);
            ScopedSetBool disable_history(c.history_disabled());

            func(parser, c);
        };
        if (*bufnames == "*")
        {
            // copy buffer list as we might be mutating the buffer list
            // in the loop.
            auto ptrs = BufferManager::instance() |
                transform(std::mem_fn(&std::unique_ptr<Buffer>::get));
            Vector<SafePtr<Buffer>> buffers{ptrs.begin(), ptrs.end()};
            for (auto buffer : buffers)
                context_wrap_for_buffer(*buffer);
        }
        else
            for (auto& name : split(*bufnames, ','))
                context_wrap_for_buffer(BufferManager::instance().get_buffer(name));
        return;
    }

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
                              Context::Flags::Transient,
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

    ScopedSetBool disable_hooks(c.hooks_disabled(), no_hooks);
    ScopedSetBool disable_keymaps(c.keymaps_disabled(), no_keymaps);
    ScopedSetBool disable_history(c.history_disabled());

    if (parser.get_switch("itersel"))
    {
        SelectionList sels{base_context->selections()};
        Vector<Selection> new_sels;
        size_t main = 0;
        size_t timestamp = c.buffer().timestamp();
        ScopedEdition edition{c};
        for (auto& sel : sels)
        {
            c.selections_write_only() = SelectionList{ sels.buffer(), sel, sels.timestamp() };
            c.selections().update();

            func(parser, c);

            if (&sels.buffer() != &c.buffer())
                throw runtime_error("the buffer has changed while iterating on selections");

            if (not draft)
            {
                update_selections(new_sels, main, c.buffer(), timestamp);
                timestamp = c.buffer().timestamp();
                for (auto& sel : c.selections())
                    new_sels.push_back(sel);
            }
        }

        if (not draft)
        {
            c.selections_write_only() = SelectionList(c.buffer(), new_sels);
            c.selections().sort_and_merge_overlapping();
        }
    }
    else
    {
        const bool collapse_jumps = not draft and (bool)parser.get_switch("collapse-jumps");
        SelectionList jump = c.selections();
        JumpList original_jump_list = collapse_jumps ? c.jump_list() : JumpList{};

        func(parser, c);

        if (collapse_jumps and c.jump_list() != original_jump_list)
        {
            c.jump_list() = std::move(original_jump_list);
            c.jump_list().push(std::move(jump));
        }
    }
}

const CommandDesc exec_string_cmd = {
    "exec",
    nullptr,
    "exec <switches> <keys>: execute given keys as if entered by user",
    context_wrap_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        context_wrap(parser, context, [](const ParametersParser& parser, Context& context) {
            KeyList keys;
            for (auto& param : parser)
            {
                KeyList param_keys = parse_keys(param);
                keys.insert(keys.end(), param_keys.begin(), param_keys.end());
            }

            ScopedEdition edition(context);
            for (auto& key : keys)
                context.input_handler().handle_key(key);
        });
    }
};

const CommandDesc eval_string_cmd = {
    "eval",
    nullptr,
    "eval <switches> <commands>...: execute commands as if entered by user",
    context_wrap_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        context_wrap(parser, context, [&](const ParametersParser& parser, Context& context) {
            String command = join(parser, ' ', false);
            CommandManager::instance().execute(command, context, shell_context);
        });
    }
};

const CommandDesc prompt_cmd = {
    "prompt",
    nullptr,
    "prompt <prompt> <command>: prompt the user to enter a text string "
    "and then executes <command>, entered text is available in the 'text' value",
    ParameterDesc{
        { { "init", { true, "set initial prompt content" } },
          { "password", { false, "Do not display entered text and clear reg after command" } },
          { "file-completion", { false, "use file completion for prompt" } },
          { "client-completion", { false, "use client completion for prompt" } },
          { "buffer-completion", { false, "use buffer completion for prompt" } },
          { "command-completion", { false, "use command completion for prompt" } } },
        ParameterDesc::Flags::None, 2, 2
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        const String& command = parser[1];
        auto initstr = parser.get_switch("init").value_or(StringView{});

        Completer completer;
        if (parser.get_switch("file-completion"))
            completer = [](const Context& context, CompletionFlags,
                           StringView prefix, ByteCount cursor_pos) -> Completions {
                auto& ignored_files = context.options()["ignored_files"].get<Regex>();
                return { 0_byte, cursor_pos,
                         complete_filename(prefix, ignored_files, cursor_pos,
                                           FilenameFlags::Expand) };
            };
        else if (parser.get_switch("client-completion"))
            completer = [](const Context& context, CompletionFlags,
                           StringView prefix, ByteCount cursor_pos) -> Completions {
                 return { 0_byte, cursor_pos,
                          ClientManager::instance().complete_client_name(prefix, cursor_pos) };
            };
        else if (parser.get_switch("buffer-completion"))
            completer = complete_buffer_name;
        else if (parser.get_switch("command-completion"))
            completer = [](const Context& context, CompletionFlags flags,
                           StringView prefix, ByteCount cursor_pos) -> Completions {
                return CommandManager::instance().complete(
                    context, flags, prefix, cursor_pos);
            };

        const auto flags = parser.get_switch("password") ?
            PromptFlags::Password : PromptFlags::None;

        // const cast so that lambda capute is mutable
        ShellContext& sc = const_cast<ShellContext&>(shell_context);
        context.input_handler().prompt(
            parser[0], initstr.str(), get_face("Prompt"),
            flags, std::move(completer),
            [=](StringView str, PromptEvent event, Context& context) mutable
            {
                if (event != PromptEvent::Validate)
                    return;
                sc.env_vars["text"] = str.str();

                ScopedSetBool disable_history{context.history_disabled()};

                CommandManager::instance().execute(command, context, sc);

                if (flags & PromptFlags::Password)
                {
                    String& str = sc.env_vars["text"];;
                    memset(str.data(), 0, (int)str.length());
                    str = "";
                }
            });
    }
};

const CommandDesc menu_cmd = {
    "menu",
    nullptr,
    "menu <switches> <name1> <commands1> <name2> <commands2>...: display a "
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
            choices.push_back(markup ? parse_display_line(parser[i])
                                     : DisplayLine{ parser[i], {} });
            commands.push_back(parser[i+1]);
            if (with_select_cmds)
                select_cmds.push_back(parser[i+2]);
        }

        context.input_handler().menu(std::move(choices),
            [=](int choice, MenuEvent event, Context& context) {
                ScopedSetBool disable_history{context.history_disabled()};

                if (event == MenuEvent::Validate and choice >= 0 and choice < commands.size())
                  CommandManager::instance().execute(commands[choice], context, shell_context);
                if (event == MenuEvent::Select and choice >= 0 and choice < select_cmds.size())
                  CommandManager::instance().execute(select_cmds[choice], context, shell_context);
            });
    }
};

const CommandDesc on_key_cmd = {
    "on-key",
    nullptr,
    "on-key <command>: wait for next user key then and execute <command>, "
    "with key availabe in the `key` value",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 1, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        String command = parser[0];

        // const cast so that the lambda capute is mutable
        ShellContext& sc = const_cast<ShellContext&>(shell_context);
        context.input_handler().on_next_key(
            KeymapMode::None, [=](Key key, Context& context) mutable {
            sc.env_vars["key"] = key_to_str(key);
            ScopedSetBool disable_history{context.history_disabled()};

            CommandManager::instance().execute(command, context, sc);
        });
    }
};

const CommandDesc info_cmd = {
    "info",
    nullptr,
    "info <switches> <params>...: display an info box with the params as content",
    ParameterDesc{
        { { "anchor",    { true, "set info anchoring <line>.<column>" } },
          { "placement", { true, "set placement relative to anchor (above, below)" } },
          { "title",     { true, "set info title" } } },
        ParameterDesc::Flags::None, 0, 1
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not context.has_client())
            return;

        context.client().info_hide();
        if (parser.positional_count() > 0)
        {
            InfoStyle style = InfoStyle::Prompt;
            BufferCoord pos;
            if (auto anchor = parser.get_switch("anchor"))
            {
                auto dot = find(*anchor, '.');
                if (dot == anchor->end())
                    throw runtime_error("expected <line>.<column> for anchor");

                pos = BufferCoord{str_to_int({anchor->begin(), dot})-1,
                                str_to_int({dot+1, anchor->end()})-1};
                style = InfoStyle::Inline;

                if (auto placement = parser.get_switch("placement"))
                {
                    if (*placement == "above")
                        style = InfoStyle::InlineAbove;
                    else if (*placement == "below")
                        style = InfoStyle::InlineBelow;
                    else
                        throw runtime_error(format("invalid placement '{}'", *placement));
                }
            }
            auto title = parser.get_switch("title").value_or(StringView{});
            context.client().info_show(title.str(), parser[0], pos, style);
        }
    }
};

const CommandDesc try_catch_cmd = {
    "try",
    nullptr,
    "try <cmds> [catch <error_cmds>]: execute <cmds> in current context.\n"
    "if an error is raised and <error_cmds> is specified, execute it; "
    "The error is not propagated further.",
    ParameterDesc{{}, ParameterDesc::Flags::None, 1, 3},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        if (parser.positional_count() == 2)
            throw wrong_argument_count();

        const bool do_catch = parser.positional_count() == 3;
        if (do_catch and parser[1] != "catch")
            throw runtime_error("usage: try <commands> [catch <on error commands>]");

        CommandManager& command_manager = CommandManager::instance();
        try
        {
            command_manager.execute(parser[0], context, shell_context);
        }
        catch (Kakoune::runtime_error& e)
        {
            if (do_catch)
                command_manager.execute(parser[2], context, shell_context);
        }
    }
};

static Completions complete_face(const Context&, CompletionFlags flags,
                                 const String& prefix, ByteCount cursor_pos)
{
    return {0_byte, cursor_pos,
            FaceRegistry::instance().complete_alias_name(prefix, cursor_pos)};
}

const CommandDesc set_face_cmd = {
    "set-face",
    "face",
    "set-face <name> <facespec>: set face <name> to refer to <facespec>\n"
    "\n"
    "facespec format is <fg color>[,<bg color>][+<attributes>]\n"
    "colors are either a color name, or rgb:###### values.\n"
    "attributes is a combination of:\n"
    "    u: underline, i: italic, b: bold, r: reverse,\n"
    "    B: blink, d: dim, e: exclusive\n"
    "facespec can as well just be the name of another face" ,
    ParameterDesc{{}, ParameterDesc::Flags::None, 2, 2},
    CommandFlags::None,
    CommandHelper{},
    make_completer(complete_face, complete_face),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        FaceRegistry::instance().register_alias(parser[0], parser[1], true);

        for (auto& client : ClientManager::instance())
            client->force_redraw();
    }
};

const CommandDesc rename_client_cmd = {
    "rename-client",
    "nc",
    "rename-client <name>: set current client name to <name>",
    single_name_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (ClientManager::instance().validate_client_name(parser[0]))
            context.set_name(parser[0]);
        else if (context.name() != parser[0])
            throw runtime_error(format("client name '{}' is not unique", parser[0]));
    }
};

const CommandDesc set_register_cmd = {
    "set-register",
    "reg",
    "set-register <name> <value>: set register <name> to <value>",
    ParameterDesc{{}, ParameterDesc::Flags::None, 2, 2},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        RegisterManager::instance()[parser[0]] = ConstArrayView<String>(parser[1]);
    }
};

const CommandDesc select_cmd = {
    "select",
    nullptr,
    "select <selections_desc>: select given selections",
    ParameterDesc{{}, ParameterDesc::Flags::None, 1, 1},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        context.selections_write_only() = selection_list_from_string(context.buffer(), parser[0]);
    }
};

const CommandDesc change_directory_cmd = {
    "change-directory",
    "cd",
    "change-directory [<directory>]: change the server's working directory to <directory>, or the home directory if unspecified",
    single_optional_name_param,
    CommandFlags::None,
    CommandHelper{},
    make_completer(
         [](const Context& context, CompletionFlags flags,
            const String& prefix, ByteCount cursor_pos) -> Completions {
             return { 0_byte, cursor_pos,
                      complete_filename(prefix,
                                        context.options()["ignored_files"].get<Regex>(),
                                        cursor_pos, FilenameFlags::OnlyDirectories) };
        }),
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        StringView target = parser.positional_count() == 1 ? StringView{parser[0]} : "~";
        if (chdir(parse_filename(target).c_str()) != 0)
            throw runtime_error(format("cannot change to directory '{}'", target));
        for (auto& buffer : BufferManager::instance())
            buffer->update_display_name();
    }
};

const CommandDesc rename_session_cmd = {
    "rename-session",
    nullptr,
    "rename-session <name>: change remote session name",
    ParameterDesc{{}, ParameterDesc::Flags::None, 1, 1},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        if (not Server::instance().rename_session(parser[0]))
            throw runtime_error(format("Cannot rename current session: '{}' may be already in use", parser[0]));
    }
};

}

void register_commands()
{
    CommandManager& cm = CommandManager::instance();
    cm.register_command("nop", [](const ParametersParser&, Context&, const ShellContext&){}, "do nothing", {});

    auto register_command = [&](const CommandDesc& c)
    {
        cm.register_command(c.name, c.func, c.docstring, c.params, c.flags, c.helper, c.completer);
        if (c.alias)
            GlobalScope::instance().aliases().add_alias(c.alias, c.name);
    };

    register_command(edit_cmd);
    register_command(force_edit_cmd);
    register_command(write_cmd);
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
    register_command(add_highlighter_cmd);
    register_command(remove_highlighter_cmd);
    register_command(add_hook_cmd);
    register_command(remove_hook_cmd);
    register_command(define_command_cmd);
    register_command(alias_cmd);
    register_command(unalias_cmd);
    register_command(echo_cmd);
    register_command(debug_cmd);
    register_command(source_cmd);
    register_command(set_option_cmd);
    register_command(unset_option_cmd);
    register_command(declare_option_cmd);
    register_command(map_key_cmd);
    register_command(unmap_key_cmd);
    register_command(exec_string_cmd);
    register_command(eval_string_cmd);
    register_command(prompt_cmd);
    register_command(menu_cmd);
    register_command(on_key_cmd);
    register_command(info_cmd);
    register_command(try_catch_cmd);
    register_command(set_face_cmd);
    register_command(rename_client_cmd);
    register_command(set_register_cmd);
    register_command(select_cmd);
    register_command(change_directory_cmd);
    register_command(rename_session_cmd);
}

}
