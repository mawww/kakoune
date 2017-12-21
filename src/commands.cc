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
#include "insert_completer.hh"
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

#include <cstring>
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
    template<typename C, typename... R,
             typename = std::enable_if_t<not std::is_base_of<PerArgumentCommandCompleter<>,
                                         std::remove_reference_t<C>>::value>>
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
const ParameterDesc single_param{ {}, ParameterDesc::Flags::None, 1, 1 };
const ParameterDesc single_optional_param{ {}, ParameterDesc::Flags::None, 0, 1 };

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
       context, prefix.substr(0, cursor_pos));
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
    const auto flags = (no_hooks ? Buffer::Flags::NoHooks : Buffer::Flags::None) |
       (parser.get_switch("debug") ? Buffer::Flags::Debug : Buffer::Flags::None);

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
            buffer = open_fifo(name, *fifo, flags, (bool)parser.get_switch("scroll"));
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

    Buffer* current_buffer = context.has_buffer() ? &context.buffer() : nullptr;

    const size_t param_count = parser.positional_count();
    if (current_buffer and (buffer != current_buffer or param_count > 1))
        context.push_jump();

    if (buffer != current_buffer)
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
      { "debug",    { false, "create buffer as debug output" } },
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

template<bool force = false>
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
    write_buffer_to_file(buffer, filename, force);
    context.hooks().run_hook("BufWritePost", filename, context);
}

const CommandDesc write_cmd = {
    "write",
    "w",
    "write [filename]: write the current buffer to its file "
    "or to [filename] if specified",
    single_optional_param,
    CommandFlags::None,
    CommandHelper{},
    filename_completer,
    write_buffer,
};

const CommandDesc force_write_cmd = {
    "write!",
    "w!",
    "write [filename]: write the current buffer to its file "
    "or to [filename] if specified, even when the file is write protected",
    single_optional_param,
    CommandFlags::None,
    CommandHelper{},
    filename_completer,
    write_buffer<true>,
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
void quit(const ParametersParser& parser, Context& context, const ShellContext&)
{
    if (not force and ClientManager::instance().count() == 1)
        ensure_all_buffers_are_saved();

    const int status = parser.positional_count() > 0 ? str_to_int(parser[0]) : 0;
    ClientManager::instance().remove_client(context.client(), true, status);
}

const CommandDesc quit_cmd = {
    "quit",
    "q",
    "quit current client, and the kakoune session if the client is the last "
    "(if not running in daemon mode). An optional integer parameter can set "
    "the client exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    quit<false>
};

const CommandDesc force_quit_cmd = {
    "quit!",
    "q!",
    "quit current client, and the kakoune session if the client is the last "
    "(if not running in daemon mode). force quit even if the client is the "
    "last and some buffers are not saved. An optional integer parameter can "
    "set the client exit status",
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
    write_buffer({{}, {}}, context, shell_context);
    quit<force>(parser, context, shell_context);
}

const CommandDesc write_quit_cmd = {
    "write-quit",
    "wq",
    "write current buffer and quit current client. An optional integer "
    "parameter can set the client exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    write_quit<false>
};

const CommandDesc force_write_quit_cmd = {
    "write-quit!",
    "wq!",
    "write current buffer and quit current client, even if other buffers are "
    "not saved. An optional integer parameter can set the client exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    write_quit<true>
};

const CommandDesc write_all_quit_cmd = {
    "write-all-quit",
    "waq",
    "write all buffers associated to a file and quit current client. An "
    "optional integer parameter can set the client exit status",
    { {}, ParameterDesc::Flags::SwitchesAsPositional, 0, 1 },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        write_all_buffers(context);
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
}

const CommandDesc delete_buffer_cmd = {
    "delete-buffer",
    "db",
    "delete-buffer [name]: delete current buffer or the buffer named <name> if given",
    single_optional_param,
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
    single_optional_param,
    CommandFlags::None,
    CommandHelper{},
    buffer_completer,
    delete_buffer<true>
};

const CommandDesc rename_buffer_cmd = {
    "rename-buffer",
    nullptr,
    "rename-buffer <name>: change current buffer name",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (not context.buffer().set_name(parser[0]))
            throw runtime_error(format("unable to change buffer name to '{}'", parser[0]));
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
           return { 0_byte, pos_in_token, complete(path, pos_in_token, highlighter_scopes) };

        StringView scope{path.begin(), sep_it};
        HighlighterGroup* root = nullptr;
        if (scope == "shared")
            root = &DefinedHighlighters::instance();
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
        return { 0_byte, name.length(), complete(name, pos_in_token, HighlighterRegistry::instance() | transform(std::mem_fn(&HighlighterRegistry::Item::key))) };
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
    auto* root = (scope == "shared") ? static_cast<HighlighterGroup*>(&DefinedHighlighters::instance())
                                     : static_cast<HighlighterGroup*>(&get_scope(scope, context).highlighters().group());
    if (sep_it != path.end())
        return root->get_child(StringView{sep_it+1, path.end()});
    return *root;
}

const CommandDesc add_highlighter_cmd = {
    "add-highlighter",
    "addhl",
    "add-highlighter <path> <type> <type params>...: add an highlighter to the group identified by <path>\n"
    "    <path> is a '/' delimited path of highlighters, starting with either\n"
    "   'global', 'buffer', 'window' or 'shared'",
    ParameterDesc{ {}, ParameterDesc::Flags::SwitchesAsPositional, 2 },
    CommandFlags::None,
    [](const Context& context, CommandParameters params) -> String
    {
        if (params.size() > 1)
        {
            HighlighterRegistry& registry = HighlighterRegistry::instance();
            auto it = registry.find(params[1]);
            if (it != registry.end())
                return format("{}:\n{}", params[1], indent(it->value.docstring));
        }
        return "";
    },
    highlighter_cmd_completer<true>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        auto begin = parser.begin();
        StringView path = *begin++;
        StringView name = *begin++;
        Vector<String> highlighter_params;
        for (; begin != parser.end(); ++begin)
            highlighter_params.push_back(*begin);

        auto it = registry.find(name);
        if (it == registry.end())
            throw runtime_error(format("No such highlighter factory '{}'", name));
        get_highlighter(context, path).add_child(it->value.factory(highlighter_params));

        // TODO: better, this will fail if we touch scopes highlighters that impact multiple windows
        if (context.has_window())
            context.window().force_redraw();
    }
};

const CommandDesc remove_highlighter_cmd = {
    "remove-highlighter",
    "rmhl",
    "remove-highlighter <path>: remove highlighter identified by <path>",
    { {}, ParameterDesc::Flags::None, 1, 1 },
    CommandFlags::None,
    CommandHelper{},
    highlighter_cmd_completer<false>,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        StringView path = parser[0];
        auto rev_path = path | reverse();
        auto sep_it = find(rev_path, '/');
        if (sep_it == rev_path.end())
            return;
        get_highlighter(context, {path.begin(), sep_it.base()}).remove_child({sep_it.base(), path.end()});

        if (context.has_window())
            context.window().force_redraw();
    }
};

static constexpr auto hooks = {
    "BufCreate", "BufNewFile", "BufOpenFile", "BufClose", "BufWritePost",
    "BufWritePre", "BufOpenFifo", "BufCloseFifo", "BufReadFifo", "BufSetOption",
    "InsertBegin", "InsertChar", "InsertDelete", "InsertEnd", "InsertIdle", "InsertKey",
    "InsertMove", "InsertCompletionHide", "InsertCompletionShow", "InsertCompletionSelect",
    "KakBegin", "KakEnd", "FocusIn", "FocusOut", "RuntimeError", "PromptIdle",
    "NormalBegin", "NormalEnd", "NormalIdle", "NormalKey", "InputModeChange", "RawKey",
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
    "<scope> can be:\n"
    "  * global: hook is executed for any buffer or window\n"
    "  * buffer: hook is executed only for the current buffer\n"
    "            (and any window for that buffer)\n"
    "  * window: hook is executed only for the current window\n",
    ParameterDesc{
        { { "group", { true, "set hook group, see remove-hooks" } } },
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

        Regex regex{parser[2], RegexCompileFlags::Optimize};
        const String& command = parser[3];
        auto group = parser.get_switch("group").value_or(StringView{});
        get_scope(parser[0], context).hooks().add_hook(parser[1], group.str(), std::move(regex), command);
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
            for (auto&& candidate : output | split<StringView>('\n'))
                candidates.push_back(candidate.str());

            return Completions{ 0_byte, pos_in_token, std::move(candidates) };
        };
    }
    else if (auto shell_cmd_opt = parser.get_switch("shell-candidates"))
    {
        String shell_cmd = shell_cmd_opt->str();
        Vector<std::pair<String, UsedLetters>, MemoryDomain::Completion> candidates;
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
                    candidates.emplace_back(c.str(), used_letters(c));
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
            auto greater = [](auto& lhs, auto& rhs) { return rhs < lhs; };
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

    auto docstring = trim_whitespaces(parser.get_switch("docstring").value_or(StringView{}));

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
          { "shell-completion",   { true,  "complete parameters using the given shell-script" } },
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
    "alias <scope> <alias> <command>: alias <alias> to <command> in <scope>",
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
        { { "markup", { false, "parse markup" } },
          { "debug", { false, "write to debug buffer instead of status line" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart
    },
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        String message = fix_atom_text(join(parser, ' ', false));
        if (parser.get_switch("debug"))
            write_to_debug_buffer(message);
        else if (parser.get_switch("markup"))
            context.print_status(parse_display_line(message));
        else
            context.print_status({ std::move(message), get_face("StatusLine") });
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

const CommandDesc debug_cmd = {
    "debug",
    nullptr,
    "debug <command>: write some debug informations in the debug buffer\n"
    "existing commands: info, buffers, options, memory, shared-strings, profile-hash-maps, faces",
    ParameterDesc{{}, ParameterDesc::Flags::SwitchesOnlyAtStart, 1},
    CommandFlags::None,
    CommandHelper{},
    make_completer(
        [](const Context& context, CompletionFlags flags,
           const String& prefix, ByteCount cursor_pos) -> Completions {
               auto c = {"info", "buffers", "options", "memory", "shared-strings",
                         "profile-hash-maps", "faces", "mappings"};
               return { 0_byte, cursor_pos, complete(prefix, cursor_pos, c) };
    }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        if (parser[0] == "info")
        {
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
        else if (parser[0] == "profile-hash-maps")
        {
            profile_hash_maps();
        }
        else if (parser[0] == "faces")
        {
            write_to_debug_buffer("Faces:");
            for (auto& face : FaceRegistry::instance().aliases())
                write_to_debug_buffer(format(" * {}: {}", face.key, face.value.face));
        }
        else if (parser[0] == "mappings")
        {
            auto& keymaps = context.keymaps();
            auto modes = {"normal", "insert", "prompt", "menu",
                          "goto", "view", "user", "object"};
            write_to_debug_buffer("Mappings:");
            for (auto& mode : modes)
            {
                KeymapMode m = parse_keymap_mode(mode);
                for (auto& key : keymaps.get_mapped_keys(m))
                    write_to_debug_buffer(format(" * {} {}: {}",
                                          mode, key_to_str(key),
                                          keymaps.get_mapping(key, m).docstring));
            }
        }
        else
            throw runtime_error(format("unknown debug command '{}'", parser[0]));
    }
};

const CommandDesc source_cmd = {
    "source",
    nullptr,
    "source <filename>: execute commands contained in <filename>",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    filename_completer,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        String path = real_path(parse_filename(parser[0]));
        String file_content = read_file(path, true);
        try
        {
            CommandManager::instance().execute(file_content, context,
                                               {{}, {{"source", path}}});
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
    "set-option <switches> <scope> <name> <value>: set option <name> in <scope> to <value>\n"
    "<scope> can be global, buffer, window, or current which refers to the narrowest "
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

Completions complete_option(const Context& context, CompletionFlags,
                            CommandParameters params, size_t token_to_complete,
                            ByteCount pos_in_token)
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
}

const CommandDesc unset_option_cmd = {
    "unset-option",
    "unset",
    "unset-option <scope> <name>: remove <name> option from scope, falling back on parent scope value\n"
    "<scope> can be buffer, window, or current which refers to the narrowest "
    "scope the option is set in",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 2, 2 },
    CommandFlags::None,
    option_doc_helper,
    complete_option,
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        auto& options = get_options(parser[0], context, parser[1]);
        if (&options == &GlobalScope::instance().options())
            throw runtime_error("Cannot unset options in global scope");
        options.unset_option(parser[1]);
    }
};

const CommandDesc update_option_cmd = {
    "update-option",
    nullptr,
    "update-option <scope> <name>: update <name> option from scope\n"
    "some option types, such as line-descs or range-descs can be updated to latest buffer timestamp\n"
    "<scope> can be buffer, window, or current which refers to the narrowest "
    "scope the option is set in",
    ParameterDesc{ {}, ParameterDesc::Flags::None, 2, 2 },
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
    "declare-option <type> <name> [value]: declare option <name> of type <type>.\n"
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
    "    range-specs: list of range specs\n",
    ParameterDesc{
        { { "hidden",    { false, "do not display option name when completing" } },
          { "docstring", { true,  "specify option description" } } },
        ParameterDesc::Flags::SwitchesOnlyAtStart, 2, 3
    },
    CommandFlags::None,
    CommandHelper{},
    make_completer(
        [](const Context& context, CompletionFlags flags,
           const String& prefix, ByteCount cursor_pos) -> Completions {
               auto c = {"int", "bool", "str", "regex", "int-list", "str-list", "completions", "line-specs", "range-specs"};
               return { 0_byte, cursor_pos, complete(prefix, cursor_pos, c) };
    }),
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        Option* opt = nullptr;

        OptionFlags flags = OptionFlags::None;
        if (parser.get_switch("hidden"))
            flags = OptionFlags::Hidden;

        auto docstring = trim_whitespaces(parser.get_switch("docstring").value_or(StringView{})).str();
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
        else
            throw runtime_error(format("unknown type {}", parser[0]));

        if (parser.positional_count() == 3)
            opt->set_from_string(parser[2]);
    }
};

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
    ParameterDesc{
        { { "docstring", { true,  "specify mapping description" } } },
        ParameterDesc::Flags::None, 4, 4
    },
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
        keymaps.map_key(key[0], keymap_mode, std::move(mapping),
                        trim_whitespaces(parser.get_switch("docstring").value_or("")).str());
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
             (keymaps.get_mapping(key[0], keymap_mode).keys ==
              parse_keys(parser[3]))))
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

class RegisterRestorer
{
public:
    RegisterRestorer(char name, Context& context)
      : m_context{context}, m_name{name}
    {
        ConstArrayView<String> save = RegisterManager::instance()[name].get(context);
        m_save = Vector<String>(save.begin(), save.end());
    }

    RegisterRestorer(RegisterRestorer&& other) noexcept
        : m_context{other.m_context}, m_save{std::move(other.m_save)}, m_name{other.m_name}
    {
        other.m_name = 0;
    }

    ~RegisterRestorer()
    {
        if (m_name != 0) try
        {
            RegisterManager::instance()[m_name].set(m_context, m_save);
        }
        catch (runtime_error& e)
        {
            write_to_debug_buffer(format("Could not restore register '{}': {}",
                                         m_name, e.what()));
        }
    }

private:
    Vector<String> m_save;
    Context&       m_context;
    char           m_name;
};

template<typename Func>
void context_wrap(const ParametersParser& parser, Context& context, Func func)
{
    if ((int)(bool)parser.get_switch("buffer") +
        (int)(bool)parser.get_switch("client") +
        (int)(bool)parser.get_switch("try-client") > 1)
        throw runtime_error{"Only one of -buffer, -client or -try-client can be specified"};

    const bool no_hooks = parser.get_switch("no-hooks") or context.hooks_disabled();
    const bool no_keymaps = not parser.get_switch("with-maps");

    Vector<RegisterRestorer> saved_registers;
    for (auto& r : parser.get_switch("save-regs").value_or("/\"|^@"))
        saved_registers.emplace_back(r, context);

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
                transform(std::mem_fn(&std::unique_ptr<Buffer>::get)) |
                filter([](Buffer* buf) { return not (buf->flags() & Buffer::Flags::Debug); });
            Vector<SafePtr<Buffer>> buffers{ptrs.begin(), ptrs.end()};
            for (auto buffer : buffers)
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

            if (not draft)
            {
                if (&sels.buffer() != &c.buffer())
                    throw runtime_error("the buffer has changed while iterating on selections");

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
    "execute-keys",
    "exec",
    "execute-keys <switches> <keys>: execute given keys as if entered by user",
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
    "evaluate-commands",
    "eval",
    "evaluate-commands <switches> <commands>...: execute commands as if entered by user",
    context_wrap_params,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        context_wrap(parser, context, [&](const ParametersParser& parser, Context& context) {
            String command = join(parser, ' ', false);
            ScopedEdition edition(context);
            CommandManager::instance().execute(command, context, shell_context);
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
    "prompt <prompt> <command>: prompt the user to enter a text string "
    "and then executes <command>, entered text is available in the 'text' value",
    ParameterDesc{
        { { "init", { true, "set initial prompt content" } },
          { "password", { false, "Do not display entered text and clear reg after command" } },
          { "file-completion", { false, "use file completion for prompt" } },
          { "client-completion", { false, "use client completion for prompt" } },
          { "buffer-completion", { false, "use buffer completion for prompt" } },
          { "command-completion", { false, "use command completion for prompt" } },
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

        String on_change = parser.get_switch("on-change").value_or("").str();
        String on_abort = parser.get_switch("on-abort").value_or("").str();

        CapturedShellContext sc{shell_context};
        context.input_handler().prompt(
            parser[0], initstr.str(), {}, get_face("Prompt"),
            flags, std::move(completer),
            [=](StringView str, PromptEvent event, Context& context) mutable
            {
                if ((event == PromptEvent::Abort and on_abort.empty()) or
                    (event == PromptEvent::Change and on_change.empty()))
                    return;

                auto& text = sc.env_vars["text"_sv] = str.str();
                auto clear_password = on_scope_end([&] {
                    if (flags & PromptFlags::Password)
                        memset(text.data(), 0, (int)text.length());
                });

                ScopedSetBool disable_history{context.history_disabled()};

                StringView cmd;
                switch (event)
                {
                    case PromptEvent::Validate: cmd = command; break;
                    case PromptEvent::Change: cmd = on_change; break;
                    case PromptEvent::Abort: cmd = on_abort; break;
                }
                CommandManager::instance().execute(cmd, context, sc);
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
    "on-key <command>: wait for next user key then and execute <command>, "
    "with key available in the `key` value",
    single_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext& shell_context)
    {
        String command = parser[0];

        CapturedShellContext sc{shell_context};
        context.input_handler().on_next_key(
            KeymapMode::None, [=](Key key, Context& context) mutable {
            sc.env_vars["key"_sv] = key_to_str(key);
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
    "facespec can as well just be the name of another face",
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
    single_param,
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
    ParameterDesc{{}, ParameterDesc::Flags::SwitchesAsPositional, 2, 2},
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context& context, const ShellContext&)
    {
        RegisterManager::instance()[parser[0]].set(context, {parser[1]});
    }
};

const CommandDesc select_cmd = {
    "select",
    nullptr,
    "select <selections_desc>: select given selections\n"
    "\n"
    "selections_desc format is <anchor_line>.<anchor_column>,<cursor_line>.<cursor_column>:...",
    single_param,
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
    single_optional_param,
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
    single_param,
    CommandFlags::None,
    CommandHelper{},
    CommandCompleter{},
    [](const ParametersParser& parser, Context&, const ShellContext&)
    {
        if (not Server::instance().rename_session(parser[0]))
            throw runtime_error(format("Cannot rename current session: '{}' may be already in use", parser[0]));
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
        throw runtime_error(fix_atom_text(join(parser, ' ', false)));
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
    register_command(update_option_cmd);
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
    register_command(fail_cmd);
}

}
