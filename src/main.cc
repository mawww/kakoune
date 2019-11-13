#include "assert.hh"
#include "backtrace.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "event_manager.hh"
#include "face_registry.hh"
#include "file.hh"
#include "highlighters.hh"
#include "insert_completer.hh"
#include "json_ui.hh"
#include "ncurses_ui.hh"
#include "option_types.hh"
#include "parameters_parser.hh"
#include "ranges.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "remote.hh"
#include "scope.hh"
#include "shared_string.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "unit_tests.hh"
#include "window.hh"
#include "clock.hh"

#include <fcntl.h>
#include <locale.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

namespace Kakoune
{

extern const char* version;

struct {
    unsigned int version;
    StringView notes;
} constexpr version_notes[] = { {
        20200116,
        "» {+u}InsertCompletionHide{} parameter is now the list of inserted ranges\n"
    }, {
        20191210,
        "» {+u}ModeChange{} parameter has changed to contain push/pop\n"
        "» {+ui}Mode{+u}Begin{}/{+ui}Mode{+u}End{} hooks were removed\n"
    }, {
        20190701,
        "» {+u}%file\\{<filename>}{} expansions to read files\n"
        "» {+u}echo -to-file <filename>{} to write to file\n"
        "» completions option have an on select command instead of "
        "a docstring\n"
        "» Function key syntax do not accept lower case f anymore\n"
        "» shell quoting of list options is now opt-in with "
        "{+u}$kak_quoted_...{}\n"
    }, {
        20190120,
        "» named capture groups in regex\n"
        "» auto_complete option renamed to autocomplete\n"
    }, {
        20181027,
        "» {+u}define-commands{} {+i}-shell-completion{} and {+i}-shell-candidates{} "
        "has been renamed\n"
        "» exclusive face attributes is replaced with final "
        "(fg/bg/attr)\n"
        "» {+b}<a-M>{} (merge consecutive) moved to {+b}<a-_>{} to make {+b}<a-M>{} "
        "backward {+b}<a-m>{}\n"
        "» {+u}remove-hooks{} now takes a regex parameter\n"
    }, {
        20180904,
        "» Big breaking refactoring of various Kakoune features, "
        "configuration might need to be updated see `:doc changelog` "
        "for details\n"
        "» {+u}define-command{} {+i}-allow-override{} switch has been renamed "
        "{+i}-override{}\n"
    }, {
        20180413,
        "» {+u}ModeChange{} hook has been introduced and is expected "
        "to replace the various {+ui}Mode{+u}Begin{}/{+ui}Mode{+u}End{} hooks, "
        "consider those deprecated.\n"
        "» {+b}*{} Does not strip whitespaces anymore, use built-in "
        "{+b}_{} to strip them\n"
        "» {+b}l{} on eol will go to next line, {+b}h{} on first char will "
        "go to previous\n"
        "» selections merging behaviour is now a bit more complex "
        "again\n"
        "» {+b}x{} will only jump to next line if full line is already "
        "selected\n"
        "» {+i}WORD{} text object moved to {+b}<a-w>{} instead of {+b}W{} for "
        "consistency\n"
        "» rotate main selection moved to {+b}){}, rotate content to {+b}<a-)>{}, "
        "{+b}({} for backward\n"
        "» faces are now scoped, {+u}set-face{} command takes an additional "
        "scope parameter\n"
        "» {+b}<backtab>{} key is gone, use {+b}<s-tab>{} instead\n"
} };

void show_startup_info(Client* local_client, int last_version)
{
    const Face version_face{Color::Default, Color::Default, Attribute::Bold};
    DisplayLineList info;
    for (auto note : version_notes)
    {
        if (note.version and note.version <= last_version)
            continue;

        if (not note.version)
            info.push_back({"• Development version", version_face});
        else
        {
            const auto year = note.version / 10000;
            const auto month = (note.version / 100) % 100;
            const auto day = note.version % 100;
            info.push_back({format("• Kakoune v{}.{}{}.{}{}",
                                   year, month < 10 ? "0" : "", month, day < 10 ? "0" : "", day),
                            version_face});
        }

        for (auto&& line : note.notes | split<StringView>('\n'))
            info.push_back(parse_display_line(line, GlobalScope::instance().faces()));
    }
    if (not info.empty())
    {
        info.push_back({"See the `:doc options startup-info` to control this message",
                        Face{Color::Default, Color::Default, Attribute::Italic}});
        local_client->info_show({format("Kakoune {}", version), version_face}, info, {}, InfoStyle::Prompt);
    }
}

inline void write_stdout(StringView str) { try { write(STDOUT_FILENO, str); } catch (runtime_error&) {} }
inline void write_stderr(StringView str) { try { write(STDERR_FILENO, str); } catch (runtime_error&) {} }

String runtime_directory()
{
    char relpath[PATH_MAX+1];
    format_to(relpath, "{}../share/kak", split_path(get_kak_binary_path()).first);
    struct stat st;
    if (stat(relpath, &st) == 0 and S_ISDIR(st.st_mode))
        return real_path(relpath);

    return "/usr/share/kak";
}

String config_directory()
{
    if (StringView kak_cfg_dir = getenv("KAKOUNE_CONFIG_DIR"); not kak_cfg_dir.empty())
        return kak_cfg_dir.str();
    if (StringView xdg_cfg_home = getenv("XDG_CONFIG_HOME"); not xdg_cfg_home.empty())
        return format("{}/kak", xdg_cfg_home);
    return format("{}/.config/kak", homedir());
}

static auto main_sel_first(const SelectionList& selections)
{
    auto beg = &*selections.begin(), end = &*selections.end();
    auto main = beg + selections.main_index();
    using View = ConstArrayView<Selection>;
    return concatenated(View{main, end}, View{beg, main});
}

static const EnvVarDesc builtin_env_vars[] = { {
        "bufname", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {context.buffer().display_name()}; }
    }, {
        "buffile", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {context.buffer().name()}; }
    }, {
        "buflist", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return BufferManager::instance() | transform(&Buffer::display_name) | gather<Vector>(); }
    }, {
        "buf_line_count", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.buffer().line_count())}; }
    }, {
        "timestamp", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.buffer().timestamp())}; }
    }, {
        "history_id", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string((size_t)context.buffer().current_history_id())}; }
    }, {
        "selection", false,
        [](StringView name, const Context& context) -> Vector<String>
        { const Selection& sel = context.selections().main();
          return {content(context.buffer(), sel)}; }
    }, {
        "selections", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return context.selections_content(); }
    }, {
        "runtime", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {runtime_directory()}; }
    }, {
        "config", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {config_directory()}; }
    }, {
        "version", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {version}; }
    }, {
        "opt_", true,
        [](StringView name, const Context& context) -> Vector<String>
        { return context.options()[name.substr(4_byte)].get_as_strings(); }
    }, {
        "main_reg_", true,
        [](StringView name, const Context& context) -> Vector<String>
        { return {context.main_sel_register_value(name.substr(9_byte)).str()}; }
    }, {
        "reg_", true,
        [](StringView name, const Context& context)
        { return RegisterManager::instance()[name.substr(4_byte)].get(context) |
                     gather<Vector<String>>(); }
    }, {
        "client_env_", true,
        [](StringView name, const Context& context) -> Vector<String>
        { return {context.client().get_env_var(name.substr(11_byte)).str()}; }
    }, {
        "session", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {Server::instance().session()}; }
    }, {
        "client", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {context.name()}; }
    }, {
        "client_pid", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.client().pid())}; }
    }, {
        "client_list", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return ClientManager::instance() |
                      transform([](const std::unique_ptr<Client>& c) -> const String&
                                { return c->context().name(); }) | gather<Vector>(); }
    }, {
        "modified", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {context.buffer().is_modified() ? "true" : "false"}; }
    }, {
        "cursor_line", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.selections().main().cursor().line + 1)}; }
    }, {
        "cursor_column", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.selections().main().cursor().column + 1)}; }
    }, {
        "cursor_char_value", false,
        [](StringView name, const Context& context) -> Vector<String>
        { auto coord = context.selections().main().cursor();
          auto& buffer = context.buffer();
          return {to_string((size_t)utf8::codepoint(buffer.iterator_at(coord), buffer.end()))}; }
    }, {
        "cursor_char_column", false,
        [](StringView name, const Context& context) -> Vector<String>
        { auto coord = context.selections().main().cursor();
          return {to_string(context.buffer()[coord.line].char_count_to(coord.column) + 1)}; }
    }, {
        "cursor_display_column", false,
        [](StringView name, const Context& context) -> Vector<String>
        { auto coord = context.selections().main().cursor();
          return {to_string(get_column(context.buffer(),
                                       context.options()["tabstop"].get<int>(),
                                       coord) + 1)}; }
    }, {
        "cursor_byte_offset", false,
        [](StringView name, const Context& context) -> Vector<String>
        { auto cursor = context.selections().main().cursor();
          return {to_string(context.buffer().distance({0,0}, cursor))}; }
    }, {
        "selection_desc", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {selection_to_string(ColumnType::Byte, context.buffer(), context.selections().main())}; }
    }, {
        "selections_desc", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return main_sel_first(context.selections()) |
                     transform([&buffer=context.buffer()](const Selection& sel) {
                         return selection_to_string(ColumnType::Byte, buffer, sel);
                     }) | gather<Vector>(); }
    }, {
        "selections_char_desc", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return main_sel_first(context.selections()) |
                     transform([&buffer=context.buffer()](const Selection& sel) {
                         return selection_to_string(ColumnType::Codepoint, buffer, sel);
                     }) | gather<Vector>(); }
    }, {
        "selections_display_column_desc", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return main_sel_first(context.selections()) |
                     transform([&buffer=context.buffer(), tabstop=context.options()["tabstop"].get<int>()](const Selection& sel) {
                         return selection_to_string(ColumnType::DisplayColumn, buffer, sel, tabstop);
                     }) | gather<Vector>(); }
    }, {
        "selection_length", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(char_length(context.buffer(), context.selections().main()))}; }
    }, {
        "selections_length", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return context.selections() |
                     transform([&](const Selection& s) -> String {
                         return to_string(char_length(context.buffer(), s));
                     }) | gather<Vector<String>>(); }
    }, {
        "window_width", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.window().dimensions().column)}; }
    }, {
        "window_height", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return {to_string(context.window().dimensions().line)}; }
    }, {
        "user_modes", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return context.keymaps().user_modes(); }
    }, {
        "window_range", false,
        [](StringView name, const Context& context) -> Vector<String>
        {
            auto setup = context.window().compute_display_setup(context);
            return {format("{} {} {} {}", setup.window_pos.line, setup.window_pos.column,
                                          setup.window_range.line, setup.window_range.column)};
        }
    }, {
        "history", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return history_as_strings(context.buffer().history()); }
    }, {
        "uncommitted_modifications", false,
        [](StringView name, const Context& context) -> Vector<String>
        { return undo_group_as_strings(context.buffer().current_undo_group()); }
    }
};

void register_registers()
{
    RegisterManager& register_manager = RegisterManager::instance();

    for (auto c : StringView{"abcdefghijklmnopqrstuvwxyz\"^@"})
        register_manager.add_register(c, std::make_unique<StaticRegister>());

    for (auto c : StringView{"/|:\\"})
        register_manager.add_register(c, std::make_unique<HistoryRegister>());

    using StringList = Vector<String, MemoryDomain::Registers>;

    register_manager.add_register('%', make_dyn_reg(
        [](const Context& context)
        { return StringList{{context.buffer().display_name()}}; }));

    register_manager.add_register('.', make_dyn_reg(
        [](const Context& context) {
            auto content = context.selections_content();
            return StringList{content.begin(), content.end()};
         }));

    register_manager.add_register('#', make_dyn_reg(
        [](const Context& context) {
            const size_t count = context.selections().size();
            StringList res;
            res.reserve(count);
            for (size_t i = 1; i < count+1; ++i)
                res.push_back(to_string((int)i));
            return res;
        }));

    for (size_t i = 0; i < 10; ++i)
    {
        register_manager.add_register('0'+i, make_dyn_reg(
            [i](const Context& context) {
                StringList result;
                for (auto& sel : context.selections())
                    result.emplace_back(i < sel.captures().size() ? sel.captures()[i] : "");
                return result;
            },
            [i](Context& context, ConstArrayView<String> values) {
                if (values.empty())
                    return;

                auto& sels = context.selections();
                for (size_t sel_index = 0; sel_index < sels.size(); ++sel_index)
                {
                    auto& sel = sels[sel_index];
                    if (sel.captures().size() < i+1)
                        sel.captures().resize(i+1);
                    sel.captures()[i] = values[std::min(sel_index, values.size()-1)];
                }
            }));
    }

    register_manager.add_register('_', std::make_unique<NullRegister>());
}

void register_keymaps()
{
    auto& keymaps = GlobalScope::instance().keymaps();
    keymaps.map_key(Key::Left, KeymapMode::Normal, {'h'}, "");
    keymaps.map_key(Key::Right, KeymapMode::Normal, {'l'}, "");
    keymaps.map_key(Key::Down, KeymapMode::Normal, {'j'}, "");
    keymaps.map_key(Key::Up, KeymapMode::Normal, {'k'}, "");

    keymaps.map_key(shift(Key::Left), KeymapMode::Normal, {'H'}, "");
    keymaps.map_key(shift(Key::Right), KeymapMode::Normal, {'L'}, "");
    keymaps.map_key(shift(Key::Down), KeymapMode::Normal, {'J'}, "");
    keymaps.map_key(shift(Key::Up), KeymapMode::Normal, {'K'}, "");

    keymaps.map_key(Key::End, KeymapMode::Normal, {alt('l')}, "");
    keymaps.map_key(Key::Home, KeymapMode::Normal, {alt('h')}, "");
    keymaps.map_key(shift(Key::End), KeymapMode::Normal, {alt('L')}, "");
    keymaps.map_key(shift(Key::Home), KeymapMode::Normal, {alt('H')}, "");
}

static void check_tabstop(const int& val)
{
    if (val < 1) throw runtime_error{"tabstop should be strictly positive"};
}

static void check_indentwidth(const int& val)
{
    if (val < 0) throw runtime_error{"indentwidth should be positive or zero"};
}

static void check_scrolloff(const DisplayCoord& so)
{
    if (so.line < 0 or so.column < 0)
        throw runtime_error{"scroll offset must be positive or zero"};
}

static void check_timeout(const int& timeout)
{
    if (timeout < 50)
        throw runtime_error{"the minimum acceptable timeout is 50 milliseconds"};
}

static void check_extra_word_chars(const Vector<Codepoint, MemoryDomain::Options>& extra_chars)
{
    if (any_of(extra_chars, is_blank))
        throw runtime_error{"blanks are not accepted for extra completion characters"};
}

static void check_matching_pairs(const Vector<Codepoint, MemoryDomain::Options>& pairs)
{
    if ((pairs.size() % 2) != 0)
        throw runtime_error{"matching pairs should have a pair number of element"};
    if (not all_of(pairs, [](Codepoint cp) { return is_punctuation(cp); }))
        throw runtime_error{"matching pairs can only be punctuation"};
}

void register_options()
{
    OptionsRegistry& reg = GlobalScope::instance().option_registry();

    reg.declare_option<int, check_tabstop>("tabstop", "size of a tab character", 8);
    reg.declare_option<int, check_indentwidth>("indentwidth", "indentation width", 4);
    reg.declare_option<DisplayCoord, check_scrolloff>(
        "scrolloff", "number of lines and columns to keep visible main cursor when scrolling",
        {0,0});
    reg.declare_option("eolformat", "end of line format", EolFormat::Lf);
    reg.declare_option("BOM", "byte order mark to use when writing buffer",
                       ByteOrderMark::None);
    reg.declare_option("incsearch",
                       "incrementally apply search/select/split regex",
                       true);
    reg.declare_option("autoinfo",
                       "automatically display contextual help",
                       AutoInfo::Command | AutoInfo::OnKey);
    reg.declare_option("autocomplete",
                       "automatically display possible completions",
                       AutoComplete::Insert | AutoComplete::Prompt);
    reg.declare_option("aligntab",
                       "use tab characters when possible for alignment",
                       false);
    reg.declare_option("ignored_files",
                       "patterns to ignore when completing filenames",
                       Regex{R"(^(\..*|.*\.(o|so|a))$)"});
    reg.declare_option("disabled_hooks",
                      "patterns to disable hooks whose group is matched",
                      Regex{});
    reg.declare_option("filetype", "buffer filetype", ""_str);
    reg.declare_option("path", "path to consider when trying to find a file",
                   Vector<String, MemoryDomain::Options>({ "./", "%/", "/usr/include" }));
    reg.declare_option("completers", "insert mode completers to execute.",
                       InsertCompleterDescList({
                           InsertCompleterDesc{ InsertCompleterDesc::Filename, {} },
                           InsertCompleterDesc{ InsertCompleterDesc::Word, "all"_str }
                       }), OptionFlags::None);
    reg.declare_option("static_words", "list of words to always consider for insert word completion",
                   Vector<String, MemoryDomain::Options>{});
    reg.declare_option("autoreload",
                       "autoreload buffer when a filesystem modification is detected",
                       Autoreload::Ask);
    reg.declare_option("writemethod",
                       "how to write buffer to files",
                       WriteMethod::Overwrite);
    reg.declare_option<int, check_timeout>(
        "idle_timeout", "timeout, in milliseconds, before idle hooks are triggered", 50);
    reg.declare_option<int, check_timeout>(
        "fs_check_timeout", "timeout, in milliseconds, between file system buffer modification checks",
        500);
    reg.declare_option("ui_options",
                       "space separated list of <key>=<value> options that are "
                       "passed to and interpreted by the user interface\n"
                       "\n"
                       "The ncurses ui supports the following options:\n"
                       "    <key>:                        <value>:\n"
                       "    ncurses_assistant             clippy|cat|dilbert|none|off\n"
                       "    ncurses_status_on_top         bool\n"
                       "    ncurses_set_title             bool\n"
                       "    ncurses_enable_mouse          bool\n"
                       "    ncurses_change_colors         bool\n"
                       "    ncurses_wheel_up_button       int\n"
                       "    ncurses_wheel_down_button     int\n"
                       "    ncurses_wheel_scroll_amount   int\n"
                       "    ncurses_shift_function_key    int\n",
                       UserInterface::Options{});
    reg.declare_option("modelinefmt", "format string used to generate the modeline",
                       "%val{bufname} %val{cursor_line}:%val{cursor_char_column} {{context_info}} {{mode_info}} - %val{client}@[%val{session}]"_str);

    reg.declare_option("debug", "various debug flags", DebugFlags::None);
    reg.declare_option("readonly", "prevent buffers from being modified", false);
    reg.declare_option<Vector<Codepoint, MemoryDomain::Options>, check_extra_word_chars>(
        "extra_word_chars",
        "Additional characters to be considered as words for insert completion",
        { '_' });
    reg.declare_option<Vector<Codepoint, MemoryDomain::Options>, check_matching_pairs>(
        "matching_pairs",
        "set of pair of characters to be considered as matching pairs",
        { '(', ')', '{', '}', '[', ']', '<', '>' });
    reg.declare_option<int>("startup_info_version", "version up to which startup info changes should be hidden", 0);
}

static Client* local_client = nullptr;
static int local_client_exit = 0;
static bool convert_to_client_pending = false;

enum class UIType
{
    NCurses,
    Json,
    Dummy,
};

UIType parse_ui_type(StringView ui_name)
{
    if (ui_name == "ncurses") return UIType::NCurses;
    if (ui_name == "json") return UIType::Json;
    if (ui_name == "dummy") return UIType::Dummy;

    throw parameter_error(format("error: unknown ui type: '{}'", ui_name));
}

std::unique_ptr<UserInterface> make_ui(UIType ui_type)
{
    struct DummyUI : UserInterface
    {
        DummyUI() { set_signal_handler(SIGINT, SIG_DFL); }
        bool is_ok() const override { return true; }
        void menu_show(ConstArrayView<DisplayLine>, DisplayCoord,
                       Face, Face, MenuStyle) override {}
        void menu_select(int) override {}
        void menu_hide() override {}

        void info_show(const DisplayLine&, const DisplayLineList&, DisplayCoord, Face, InfoStyle) override {}
        void info_hide() override {}

        void draw(const DisplayBuffer&, const Face&, const Face&) override {}
        void draw_status(const DisplayLine&, const DisplayLine&, const Face&) override {}
        DisplayCoord dimensions() override { return {24,80}; }
        void set_cursor(CursorMode, DisplayCoord) override {}
        void refresh(bool) override {}
        void set_on_key(OnKeyCallback) override {}
        void set_ui_options(const Options&) override {}
    };

    switch (ui_type)
    {
        case UIType::NCurses: return std::make_unique<NCursesUI>();
        case UIType::Json: return std::make_unique<JsonUI>();
        case UIType::Dummy: return std::make_unique<DummyUI>();
    }
    throw logic_error{};
}

pid_t fork_server_to_background()
{
    if (pid_t pid = fork())
        return pid;

    setsid();
    if (fork()) // double fork to orphan the server
        exit(0);

    write_stderr(format("Kakoune forked server to background ({}), for session '{}'\n",
                        getpid(), Server::instance().session()));
    return 0;
}

std::unique_ptr<UserInterface> create_local_ui(UIType ui_type)
{
    if (ui_type != UIType::NCurses)
        return make_ui(ui_type);

    struct LocalUI : NCursesUI
    {
        LocalUI()
        {
            set_signal_handler(SIGTSTP, [](int) {
                if (ClientManager::instance().count() == 1 and
                    *ClientManager::instance().begin() == local_client)
                    NCursesUI::instance().suspend();
                else
                    convert_to_client_pending = true;
           });
        }

        ~LocalUI() override
        {
            local_client = nullptr;
            if (convert_to_client_pending or
                ClientManager::instance().empty())
                return;

            if (fork_server_to_background())
            {
                this->NCursesUI::~NCursesUI();
                exit(local_client_exit);
            }
        }
    };

    if (not isatty(0))
    {
        // move stdin to another fd, and restore tty as stdin
        int fd = dup(0);
        int tty = open("/dev/tty", O_RDONLY);
        dup2(tty, 0);
        close(tty);
        create_fifo_buffer("*stdin*", fd, Buffer::Flags::None);
    }

    return std::make_unique<LocalUI>();
}

int run_client(StringView session, StringView name, StringView client_init,
               Optional<BufferCoord> init_coord, UIType ui_type,
               bool suspend)
{
    try
    {
        Optional<int> stdin_fd;
        if (not isatty(0))
        {
            // move stdin to another fd, and restore tty as stdin
            stdin_fd = dup(0);
            int tty = open("/dev/tty", O_RDONLY);
            dup2(tty, 0);
            close(tty);
        }

        EventManager event_manager;
        RemoteClient client{session, name, make_ui(ui_type), getpid(), get_env_vars(),
                            client_init, std::move(init_coord), stdin_fd};
        stdin_fd.map(close);

        if (suspend)
            raise(SIGTSTP);
        while (not client.exit_status() and client.is_ui_ok())
            event_manager.handle_next_events(EventMode::Normal);
        return client.exit_status().value_or(-1);
    }
    catch (disconnected& e)
    {
        write_stderr(format("{}\ndisconnecting\n", e.what()));
        return -1;
    }
}

struct convert_to_client_mode
{
    String session;
    String client_name;
    String buffer_name;
    String selections;
};

enum class ServerFlags
{
    None        = 0,
    IgnoreKakrc = 1 << 0,
    Daemon      = 1 << 1,
    ReadOnly    = 1 << 2,
    StartupInfo = 1 << 3,
};
constexpr bool with_bit_ops(Meta::Type<ServerFlags>) { return true; }

int run_server(StringView session, StringView server_init,
               StringView client_init, Optional<BufferCoord> init_coord,
               ServerFlags flags, UIType ui_type, DebugFlags debug_flags,
               ConstArrayView<StringView> files)
{
    static bool terminate = false;
    if (flags & ServerFlags::Daemon)
    {
        if (session.empty())
        {
            write_stderr("-d needs a session name to be specified with -s\n");
            return -1;
        }
        if (pid_t child = fork())
        {
            write_stderr(format("Kakoune forked to background, for session '{}'\n"
                                "send SIGTERM to process {} for closing the session\n",
                                session, child));
            exit(0);
        }
        set_signal_handler(SIGTERM, [](int) { terminate = true; });
    }

    EventManager        event_manager;
    Server              server{session.empty() ? to_string(getpid()) : session.str()};

    StringRegistry      string_registry;
    GlobalScope         global_scope;
    ShellManager        shell_manager{builtin_env_vars};
    CommandManager      command_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    DefinedHighlighters defined_highlighters;
    ClientManager       client_manager;
    BufferManager       buffer_manager;

    register_options();
    register_registers();
    register_keymaps();
    register_commands();
    register_highlighters();

    global_scope.options()["debug"].set(debug_flags);

    write_to_debug_buffer("*** This is the debug buffer, where debug info will be written ***");

    const auto start_time = Clock::now();
    UnitTest::run_all_tests();

    if (debug_flags & DebugFlags::Profile)
    {
        using namespace std::chrono;
        write_to_debug_buffer(format("running the unit tests took {} ms",
                                     duration_cast<milliseconds>(Clock::now() - start_time).count()));
    }

    GlobalScope::instance().options().get_local_option("readonly").set<bool>(flags & ServerFlags::ReadOnly);

    bool startup_error = false;
    if (not (flags & ServerFlags::IgnoreKakrc)) try
    {
        Context init_context{Context::EmptyContextFlag{}};
        command_manager.execute(format("source {}/kakrc", runtime_directory()),
                                init_context);
    }
    catch (runtime_error& error)
    {
        startup_error = true;
        write_to_debug_buffer(format("error while parsing kakrc:\n"
                                     "    {}", error.what()));
    }

    if (not server_init.empty()) try
    {
        Context init_context{Context::EmptyContextFlag{}};
        command_manager.execute(server_init, init_context);
    }
    catch (runtime_error& error)
    {
        startup_error = true;
        write_to_debug_buffer(format("error while running server init commands:\n"
                                     "    {}", error.what()));
    }

    {
        Context empty_context{Context::EmptyContextFlag{}};
        global_scope.hooks().run_hook(Hook::KakBegin, session, empty_context);
    }

    if (not files.empty()) try
    {
        // create buffers in reverse order so that the first given buffer
        // is the most recently created one.
        for (auto& file : files | reverse())
        {
            try
            {
                Buffer *buffer = open_or_create_file_buffer(file);
                if (flags & ServerFlags::ReadOnly)
                    buffer->flags() |= Buffer::Flags::ReadOnly;
            }
            catch (runtime_error& error)
            {
                startup_error = true;
                write_to_debug_buffer(format("error while opening file '{}':\n"
                                             "    {}", file, error.what()));
            }
        }
    }
    catch (runtime_error& error)
    {
         write_to_debug_buffer(format("error while opening command line files: {}", error.what()));
    }

    try
    {
        if (not (flags & ServerFlags::Daemon))
        {
            local_client = client_manager.create_client(
                 create_local_ui(ui_type), getpid(), {}, get_env_vars(), client_init, std::move(init_coord),
                 [](int status) { local_client_exit = status; });

            if (startup_error and local_client)
                local_client->print_status({
                    "error during startup, see *debug* buffer for details",
                    local_client->context().faces()["Error"]
                });

            if (flags & ServerFlags::StartupInfo and local_client)
                show_startup_info(local_client, global_scope.options()["startup_info_version"].get<int>());
        }

        while (not terminate and
               (not client_manager.empty() or server.negotiating() or
                (flags & ServerFlags::Daemon)))
        {
            client_manager.redraw_clients();

            // Loop so that eventual inputs happening during the processing are handled as
            // well, avoiding unneeded redraws.
            bool allow_blocking = not client_manager.has_pending_inputs();
            while (event_manager.handle_next_events(EventMode::Normal, nullptr, allow_blocking))
            {
                client_manager.process_pending_inputs();
                allow_blocking = false;
            }
            client_manager.process_pending_inputs();

            client_manager.clear_client_trash();
            client_manager.clear_window_trash();
            buffer_manager.clear_buffer_trash();
            global_scope.option_registry().clear_option_trash();

            if (local_client and not contains(client_manager, local_client))
                local_client = nullptr;
            else if (local_client and not local_client->is_ui_ok())
            {
                ClientManager::instance().remove_client(*local_client, false, -1);
                local_client = nullptr;
                if (not client_manager.empty() and fork_server_to_background())
                    return 0;
            }
            else if (convert_to_client_pending)
            {
                kak_assert(local_client);
                auto& local_context = local_client->context();
                const String client_name = local_context.name();
                const String buffer_name = local_context.buffer().name();
                const String selections = selection_list_to_string(ColumnType::Byte, local_context.selections());

                ClientManager::instance().remove_client(*local_client, true, 0);
                client_manager.clear_client_trash();
                convert_to_client_pending = false;

                if (fork_server_to_background())
                {
                    ClientManager::instance().clear(false);
                    String session = server.session();
                    server.close_session(false);
                    throw convert_to_client_mode{ std::move(session), std::move(client_name), std::move(buffer_name), std::move(selections) };
                }
            }
        }
    }
    catch (const kill_session& kill)
    {
        local_client_exit = kill.exit_status;
    }

    {
        Context empty_context{Context::EmptyContextFlag{}};
        global_scope.hooks().run_hook(Hook::KakEnd, "", empty_context);
    }

    return local_client_exit;
}

int run_filter(StringView keystr, ConstArrayView<StringView> files, bool quiet, StringView suffix_backup)
{
    StringRegistry  string_registry;
    GlobalScope     global_scope;
    EventManager    event_manager;
    ShellManager    shell_manager{builtin_env_vars};
    RegisterManager register_manager;
    BufferManager   buffer_manager;

    register_options();
    register_registers();

    try
    {
        auto keys = parse_keys(keystr);

        auto apply_to_buffer = [&](Buffer& buffer)
        {
            try
            {
                InputHandler input_handler{
                    { buffer, Selection{{0,0}, buffer.back_coord()} },
                    Context::Flags::Draft
                };

                for (auto& key : keys)
                    input_handler.handle_key(key);
            }
            catch (runtime_error& err)
            {
                if (not quiet)
                    write_stderr(format("error while applying keys to buffer '{}': {}\n",
                                        buffer.display_name(), err.what()));
            }
        };

        for (auto& file : files)
        {
            Buffer* buffer = open_file_buffer(file, Buffer::Flags::NoHooks);
            if (not suffix_backup.empty())
                write_buffer_to_file(*buffer, buffer->name() + suffix_backup,
                                     WriteMethod::Overwrite, WriteFlags::None);
            apply_to_buffer(*buffer);
            write_buffer_to_file(*buffer, buffer->name(),
                                 WriteMethod::Overwrite, WriteFlags::None);
            buffer_manager.delete_buffer(*buffer);
        }
        if (not isatty(0))
        {
            Buffer& buffer = *buffer_manager.create_buffer(
                "*stdin*", Buffer::Flags::NoHooks, read_fd(0), InvalidTime);
            apply_to_buffer(buffer);
            write_buffer_to_fd(buffer, 1);
            buffer_manager.delete_buffer(buffer);
        }
    }
    catch (runtime_error& err)
    {
        write_stderr(format("error: {}\n", err.what()));
    }

    buffer_manager.clear_buffer_trash();
    return 0;
}

int run_pipe(StringView session)
{
    try
    {
        send_command(session, read_fd(0));
    }
    catch (disconnected& e)
    {
        write_stderr(format("{}\ndisconnecting\n", e.what()));
        return -1;
    }
    return 0;
}

void signal_handler(int signal)
{
    NCursesUI::restore_terminal();
    const char* text = nullptr;
    switch (signal)
    {
        case SIGSEGV: text = "SIGSEGV"; break;
        case SIGFPE:  text = "SIGFPE";  break;
        case SIGQUIT: text = "SIGQUIT"; break;
        case SIGTERM: text = "SIGTERM"; break;
        case SIGPIPE: text = "SIGPIPE"; break;
    }
    if (signal != SIGTERM)
    {
        auto msg = format("Received {}, exiting.\nPid: {}\nCallstack:\n{}",
                          text, getpid(), Backtrace{}.desc());
        write_stderr(msg);
        notify_fatal_error(msg);
    }

    if (Server::has_instance())
        Server::instance().close_session();
    if (BufferManager::has_instance())
        BufferManager::instance().backup_modified_buffers();

    if (signal == SIGTERM)
        exit(-1);
    else if (signal == SIGSEGV)
    {
        // generate core dump
        ::signal(SIGSEGV, SIG_DFL);
        ::kill(getpid(), SIGSEGV);
    }
    else
        abort();
}

}

int main(int argc, char* argv[])
{
    using namespace Kakoune;

    setlocale(LC_ALL, "");

    set_signal_handler(SIGSEGV, signal_handler);
    set_signal_handler(SIGFPE,  signal_handler);
    set_signal_handler(SIGQUIT, signal_handler);
    set_signal_handler(SIGTERM, signal_handler);
    set_signal_handler(SIGPIPE, [](int){});
    set_signal_handler(SIGINT, [](int){});
    set_signal_handler(SIGCHLD, [](int){});
    set_signal_handler(SIGTTOU, SIG_IGN);

    const ParameterDesc param_desc{
        SwitchMap{ { "c", { true,  "connect to given session" } },
                   { "e", { true,  "execute argument on client initialisation" } },
                   { "E", { true,  "execute argument on server initialisation" } },
                   { "n", { false, "do not source kakrc files on startup" } },
                   { "s", { true,  "set session name" } },
                   { "d", { false, "run as a headless session (requires -s)" } },
                   { "p", { true,  "just send stdin as commands to the given session" } },
                   { "f", { true,  "filter: for each file, select the entire buffer and execute the given keys" } },
                   { "i", { true, "backup the files on which a filter is applied using the given suffix" } },
                   { "q", { false, "in filter mode, be quiet about errors applying keys" } },
                   { "ui", { true, "set the type of user interface to use (ncurses, dummy, or json)" } },
                   { "l", { false, "list existing sessions" } },
                   { "clear", { false, "clear dead sessions" } },
                   { "debug", { true, "initial debug option value" } },
                   { "version", { false, "display kakoune version and exit" } },
                   { "ro", { false, "readonly mode" } },
                   { "help", { false, "display a help message and quit" } } }
    };

    try
    {
        auto show_usage = [&]()
        {
            write_stdout(format("Usage: {} [options] [file]... [+<line>[:<col>]|+:]\n\n"
                    "Options:\n"
                    "{}\n"
                    "Prefixing a positional argument with a plus (`+`) sign will place the\n"
                    "cursor at a given set of coordinates, or the end of the buffer if the plus\n"
                    "sign is followed only by a colon (`:`)\n",
                    argv[0], generate_switches_doc(param_desc.switches)));
            return 0;
        };

        const auto params = ArrayView<char*>{argv+1, argv + argc}
                          | transform([](auto* s) { return String{s}; })
                          | gather<Vector<String>>();

        if (contains(params, "--help"_sv))
            return show_usage();

        ParametersParser parser{params, param_desc};

        const bool show_help_message = (bool)parser.get_switch("help");
        if (show_help_message)
            return show_usage();

        if (parser.get_switch("version"))
        {
            write_stdout(format("Kakoune {}\n", Kakoune::version));
            return 0;
        }

        const bool list_sessions = (bool)parser.get_switch("l");
        const bool clear_sessions = (bool)parser.get_switch("clear");
        if (list_sessions or clear_sessions)
        {
            for (auto& session : list_files(session_directory()))
            {
                const bool valid = check_session(session);
                if (list_sessions)
                    write_stdout(format("{}{}\n", session, valid ? "" : " (dead)"));
                if (not valid and clear_sessions)
                    unlink(session_path(session).c_str());
            }
            return 0;
        }

        if (auto session = parser.get_switch("p"))
        {
            for (auto opt : { "c", "n", "s", "d", "e", "E", "ro" })
            {
                if (parser.get_switch(opt))
                {
                    write_stderr(format("error: -{} is incompatible with -p\n", opt));
                    return -1;
                }
            }
            return run_pipe(*session);
        }

        auto client_init = parser.get_switch("e").value_or(StringView{});
        auto server_init = parser.get_switch("E").value_or(StringView{});
        const UIType ui_type = parse_ui_type(parser.get_switch("ui").value_or("ncurses"));

        if (auto keys = parser.get_switch("f"))
        {
            if (parser.get_switch("ro"))
            {
                write_stderr("error: -ro is incompatible with -f\n");
                return -1;
            }

            Vector<StringView> files;
            for (size_t i = 0; i < parser.positional_count(); ++i)
                files.emplace_back(parser[i]);

            return run_filter(*keys, files, (bool)parser.get_switch("q"),
                              parser.get_switch("i").value_or(StringView{}));
        }

        Vector<StringView> files;
        Optional<BufferCoord> init_coord;
        for (auto& name : parser)
        {
            if (not name.empty() and name[0_byte] == '+')
            {
                if (name == "+" or name  == "+:")
                {
                    client_init = client_init + "; exec gj";
                    continue;
                }
                auto colon = find(name, ':');
                if (auto line = str_to_int_ifp({name.begin()+1, colon}))
                {
                    init_coord = std::max<BufferCoord>({0,0}, {
                        *line - 1,
                        colon != name.end() ?
                            str_to_int_ifp({colon+1, name.end()}).value_or(1) - 1
                          : 0
                    });
                    continue;
                }
            }

            files.emplace_back(name);
        }

        if (auto server_session = parser.get_switch("c"))
        {
            for (auto opt : { "n", "s", "d", "E", "ro" })
            {
                if (parser.get_switch(opt))
                {
                    write_stderr(format("error: -{} is incompatible with -c\n", opt));
                    return -1;
                }
            }
            String new_files;
            for (auto name : files) {
                new_files += format("edit '{}'", escape(real_path(name), "'", '\\'));
                if (init_coord) {
                    new_files += format(" {} {}", init_coord->line + 1, init_coord->column + 1);
                    init_coord.reset();
                }
                new_files += ";";
            }

            return run_client(*server_session, {}, new_files + client_init, init_coord, ui_type, false);
        }
        else
        {
            StringView session = parser.get_switch("s").value_or(StringView{});
            try
            {
                auto ignore_kakrc = (bool)parser.get_switch("n");
                auto flags = (ignore_kakrc                                ? ServerFlags::IgnoreKakrc : ServerFlags::None) |
                             (parser.get_switch("d")                      ? ServerFlags::Daemon      : ServerFlags::None) |
                             (parser.get_switch("ro")                     ? ServerFlags::ReadOnly    : ServerFlags::None) |
                             ((argc == 1 or (ignore_kakrc and argc == 2))
                              and isatty(0)                               ? ServerFlags::StartupInfo : ServerFlags::None);
                auto debug_flags = option_from_string(Meta::Type<DebugFlags>{}, parser.get_switch("debug").value_or(""));
                return run_server(session, server_init, client_init, init_coord, flags, ui_type, debug_flags, files);
            }
            catch (convert_to_client_mode& convert)
            {
                return run_client(convert.session, convert.client_name,
                                  format("try %^buffer '{}'; select '{}'^; echo converted to client only mode",
                                         escape(convert.buffer_name, "'^", '\\'), convert.selections), {}, ui_type, true);
            }
        }
    }
    catch (parameter_error& error)
    {
        write_stderr(format("Error while parsing parameters: {}\n"
                            "Valid switches:\n"
                            "{}", error.what(),
                            generate_switches_doc(param_desc.switches)));
       return -1;
    }
    catch (Kakoune::exception& error)
    {
        write_stderr(format("Fatal error: {}\n", error.what()));
        return -1;
    }
    catch (std::exception& error)
    {
        write_stderr(format("uncaught exception ({}):\n{}\n", typeid(error).name(), error.what()));
        return -1;
    }
    catch (...)
    {
        write_stderr("uncaught exception");
        return -1;
    }
    return 0;
}

#if defined(__ELF__)
asm(R"(
.pushsection ".debug_gdb_scripts", "MS",@progbits,1
.byte 4
.ascii "kakoune-inline-gdb.py\n"
.ascii "import os.path\n"
.ascii "sys.path.insert(0, os.path.dirname(gdb.current_objfile().filename) + '/../share/kak/gdb/')\n"
.ascii "import gdb.printing\n"
.ascii "import kakoune\n"
.ascii "gdb.printing.register_pretty_printer(gdb.current_objfile(), kakoune.build_pretty_printer())\n\0"
.popsection
)");
#endif
