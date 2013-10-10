#include "normal.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "client_manager.hh"
#include "color_registry.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "file.hh"
#include "option_manager.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "window.hh"
#include "user_interface.hh"
#include "utf8_iterator.hh"

namespace Kakoune
{

using namespace std::placeholders;

template<InsertMode mode>
void insert(Context& context, int)
{
    context.client().insert(mode);
}

void repeat_insert(Context& context, int)
{
    context.client().repeat_last_insert();
}

bool show_auto_info_ifn(const String& title, const String& info,
                        const Context& context)
{
    if (not context.options()["autoinfo"].get<bool>() or not context.has_ui())
        return false;
    ColorPair col = get_color("Information");
    DisplayCoord pos = context.window().dimensions();
    pos.column -= 1;
    context.ui().info_show(title, info, pos , col, MenuStyle::Prompt);
    return true;
}

template<typename Cmd>
void on_next_key_with_autoinfo(const Context& context, Cmd cmd,
                               const String& title, const String& info)
{
    const bool hide = show_auto_info_ifn(title, info, context);
    context.client().on_next_key([hide,cmd](Key key, Context& context) mutable {
            if (hide)
                context.ui().info_hide();
            cmd(key, context);
    });
}

template<SelectMode mode>
void goto_commands(Context& context, int line)
{
    if (line != 0)
    {
        context.push_jump();
        context.editor().select(BufferCoord{line - 1, 0});
        if (context.has_window())
            context.window().center_selection();
    }
    else
    {
        on_next_key_with_autoinfo(context, [](Key key, Context& context) {
            if (key.modifiers != Key::Modifiers::None)
                return;

            Editor& editor = context.editor();
            switch (tolower(key.key))
            {
            case 'g':
            case 'k':
                context.push_jump();
                editor.select(BufferCoord{0,0}, mode);
                break;
            case 'l':
                editor.select(select_to_eol, mode);
                break;
            case 'h':
                editor.select(select_to_eol_reverse, mode);
                break;
            case 'j':
            {
                context.push_jump();
                editor.select({editor.buffer().line_count() - 1, 0}, mode);
                break;
            }
            case 'e':
                context.push_jump();
                editor.select(editor.buffer().back_coord(), mode);
                break;
            case 't':
                if (context.has_window())
                {
                    auto line = context.window().position().line;
                    editor.select({line, 0}, mode);
                }
                break;
            case 'b':
                if (context.has_window())
                {
                    auto& window = context.window();
                    auto line = window.position().line + window.dimensions().line - 1;
                    editor.select({line, 0}, mode);
                }
                break;
            case 'c':
                if (context.has_window())
                {
                    auto& window = context.window();
                    auto line = window.position().line + window.dimensions().line / 2;
                    editor.select({line, 0}, mode);
                }
                break;
            case 'a':
            {
                auto& buffer_manager = BufferManager::instance();
                auto it = buffer_manager.begin();
                if (it->get() == &context.buffer() and ++it == buffer_manager.end())
                    break;
                context.push_jump();
                auto& client_manager = ClientManager::instance();
                context.change_editor(client_manager.get_unused_window_for_buffer(**it));
                break;
            }
            case 'f':
            {
                const Range& sel = context.editor().main_selection();
                const Buffer& buffer = context.buffer();
                String filename = content(buffer, sel);
                static constexpr char forbidden[] = { '\'', '\\', '\0' };
                for (auto c : forbidden)
                    if (contains(filename, c))
                        return;

                auto paths = context.options()["path"].get<std::vector<String>>();
                const String& buffer_name = buffer.name();
                auto it = find(reversed(buffer_name), '/');
                if (it != buffer_name.rend())
                    paths.insert(paths.begin(), String{buffer_name.begin(), it.base()});

                String path = find_file(filename, paths);
                if (path.empty())
                    throw runtime_error("unable to find file '" + filename + "'");
                CommandManager::instance().execute("edit '" + path + "'", context);
                break;
            }
            }
        }, "goto",
        "g,k:  buffer top   \n"
        "l:    line end     \n"
        "h:    line begin   \n"
        "j:    buffer bottom\n"
        "e:    buffer end   \n"
        "t:    window top   \n"
        "b:    window bottom\n"
        "c:    window center\n"
        "a:    last buffer  \n"
        "f:    file         \n");
    }
}

void view_commands(Context& context, int param)
{
    on_next_key_with_autoinfo(context, [param](Key key, Context& context) {
        if (key.modifiers != Key::Modifiers::None or not context.has_window())
            return;

        Window& window = context.window();
        switch (tolower(key.key))
        {
        case 'v':
        case 'c':
            context.window().center_selection();
            break;
        case 't':
            context.window().display_selection_at(0);
            break;
        case 'b':
            context.window().display_selection_at(window.dimensions().line-1);
            break;
        case 'h':
            context.window().scroll(-std::max<CharCount>(1, param));
            break;
        case 'j':
            context.window().scroll( std::max<LineCount>(1, param));
            break;
        case 'k':
            context.window().scroll(-std::max<LineCount>(1, param));
            break;
        case 'l':
            context.window().scroll( std::max<CharCount>(1, param));
            break;
        }
    }, "view",
    "v,c:  center cursor   \n"
    "t:    cursor on top   \n"
    "b:    cursor on bottom\n"
    "h:    scroll left     \n"
    "j:    scroll down     \n"
    "k:    scroll up       \n"
    "l:    scroll right    \n");
}

void replace_with_char(Context& context, int)
{
    on_next_key_with_autoinfo(context, [](Key key, Context& context) {
        if (not isprint(key.key))
            return;
        Editor& editor = context.editor();
        SelectionList sels = editor.selections();
        auto restore_sels = on_scope_end([&]{ editor.select(std::move(sels)); });
        editor.multi_select(std::bind(select_all_matches, _1, _2, Regex{"."}));
        editor.insert(codepoint_to_str(key.key), InsertMode::Replace);
    }, "replace with char", "enter char to replace with\n");
}

Codepoint to_lower(Codepoint cp) { return tolower(cp); }
Codepoint to_upper(Codepoint cp) { return toupper(cp); }

Codepoint swap_case(Codepoint cp)
{
    Codepoint res = std::tolower(cp);
    return res == cp ? std::toupper(cp) : res;
}

template<Codepoint (*func)(Codepoint)>
void for_each_char(Context& context, int)
{
    Editor& editor = context.editor();
    std::vector<String> sels = editor.selections_content();
    for (auto& sel : sels)
    {
        for (auto& c : sel)
            c = func(c);
    }
    editor.insert(sels, InsertMode::Replace);
}

void command(Context& context, int)
{
    context.client().prompt(
        ":", get_color("Prompt"),
        std::bind(&CommandManager::complete, &CommandManager::instance(), _1, _2, _3),
        [](const String& cmdline, PromptEvent event, Context& context) {
             if (event == PromptEvent::Validate)
                 CommandManager::instance().execute(cmdline, context);
        });
}

void pipe(Context& context, int)
{
    context.client().prompt("pipe:", get_color("Prompt"), complete_nothing,
        [](const String& cmdline, PromptEvent event, Context& context)
        {
            if (event != PromptEvent::Validate)
                return;

            String real_cmd;
            if (cmdline.empty())
                real_cmd = RegisterManager::instance()['|'].values(context)[0];
            else
            {
                RegisterManager::instance()['|'] = cmdline;
                real_cmd = cmdline;
            }

            if (real_cmd.empty())
                return;

            Editor& editor = context.editor();
            std::vector<String> strings;
            for (auto& sel : context.editor().selections())
            {
                auto str = content(context.buffer(), sel);
                bool insert_eol = str.back() != '\n';
                if (insert_eol)
                    str += '\n';
                str = ShellManager::instance().pipe(str, real_cmd, context,
                                                    {}, EnvVarMap{});
                if (insert_eol and str.back() == '\n')
                    str = str.substr(0, str.length()-1);
                strings.push_back(str);
            }
            editor.insert(strings, InsertMode::Replace);
        });
}

template<SelectMode mode, Direction direction>
void search(Context& context, int)
{
    const char* prompt = direction == Forward ? "search:" : "reverse search:";
    DynamicSelectionList selections{context.buffer(), context.editor().selections()};
    context.client().prompt(prompt, get_color("Prompt"), complete_nothing,
        [selections](const String& str, PromptEvent event, Context& context) {
            try
            {
                context.editor().select(selections);

                if (event == PromptEvent::Abort)
                    return;

                Regex ex{str};
                context.client().set_prompt_colors(get_color("Prompt"));
                if (event == PromptEvent::Validate)
                {
                    if (str.empty())
                        ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
                    else
                        RegisterManager::instance()['/'] = str;
                    context.push_jump();
                }
                else if (str.empty() or not context.options()["incsearch"].get<bool>())
                    return;

                context.editor().select(std::bind(select_next_match<direction>, _1, _2, ex), mode);
            }
            catch (boost::regex_error& err)
            {
                if (event == PromptEvent::Validate)
                    throw runtime_error("regex error: "_str + err.what());
                else
                    context.client().set_prompt_colors(get_color("Error"));
            }
            catch (runtime_error&)
            {
                context.editor().select(selections);
                // only validation should propagate errors,
                // incremental search should not.
                if (event == PromptEvent::Validate)
                    throw;
            }
        });
}

template<SelectMode mode, Direction direction>
void search_next(Context& context, int param)
{
    const String& str = RegisterManager::instance()['/'].values(context)[0];
    if (not str.empty())
    {
        try
        {
            Regex ex{str};
            do {
                context.editor().select(std::bind(select_next_match<direction>, _1, _2, ex), mode);
            } while (--param > 0);
        }
        catch (boost::regex_error& err)
        {
            throw runtime_error("regex error: "_str + err.what());
        }
    }
    else
        throw runtime_error("no search pattern");
}

template<bool smart>
void use_selection_as_search_pattern(Context& context, int)
{
    std::vector<String> patterns;
    auto& sels = context.editor().selections();
    const auto& buffer = context.buffer();
    for (auto& sel : sels)
    {
        auto begin = utf8::make_iterator(buffer.iterator_at(sel.min()));
        auto end   = utf8::make_iterator(buffer.iterator_at(sel.max()))+1;
        auto content = "\\Q" + String{begin.base(), end.base()} + "\\E";
        if (smart)
        {
            if (begin == buffer.begin() or (is_word(*begin) and not is_word(*(begin-1))))
                content = "\\b" + content;
            if (end == buffer.end() or (is_word(*(end-1)) and not is_word(*end)))
                content = content + "\\b";
        }
        patterns.push_back(std::move(content));
    }
    RegisterManager::instance()['/'] = patterns;
}

void yank(Context& context, int)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.print_status({ "yanked " + to_string(context.editor().selections().size()) +
                           " selections", get_color("Information") });
}

void cat_yank(Context& context, int)
{
    auto sels = context.editor().selections_content();
    String str;
    for (auto& sel : sels)
        str += sel;
    RegisterManager::instance()['"'] = memoryview<String>(str);
    context.print_status({ "concatenated and yanked " +
                           to_string(sels.size()) + " selections", get_color("Information") });
}

void erase_selections(Context& context, int)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.editor().erase();
}

void change(Context& context, int param)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    insert<InsertMode::Replace>(context, param);
}

static InsertMode adapt_for_linewise(InsertMode mode)
{
    if (mode == InsertMode::Append)
        return InsertMode::InsertAtNextLineBegin;
    if (mode == InsertMode::Insert)
        return InsertMode::InsertAtLineBegin;
    if (mode == InsertMode::Replace)
        return InsertMode::Replace;

    kak_assert(false);
    return InsertMode::Insert;
}

template<InsertMode insert_mode>
void paste(Context& context, int)
{
    Editor& editor = context.editor();
    auto strings = RegisterManager::instance()['"'].values(context);
    InsertMode mode = insert_mode;
    for (auto& str : strings)
    {
        if (not str.empty() and str.back() == '\n')
        {
            mode = adapt_for_linewise(mode);
            break;
        }
    }
    editor.insert(strings, mode);
}

template<typename T>
void regex_prompt(Context& context, const String prompt, T on_validate)
{
    context.client().prompt(prompt, get_color("Prompt"), complete_nothing,
        [=](const String& str, PromptEvent event, Context& context) {
            if (event == PromptEvent::Validate)
            {
                try
                {
                    on_validate(str.empty() ? Regex{} : Regex{str}, context);
                }
                catch (boost::regex_error& err)
                {
                    throw runtime_error("regex error: "_str + err.what());
                }
            }
            else if (event == PromptEvent::Change)
            {
                const bool ok = Regex{str, boost::regex_constants::no_except}.status() == 0;
                context.client().set_prompt_colors(get_color(ok ? "Prompt" : "Error"));
            }
        });
}

void select_regex(Context& context, int)
{
    regex_prompt(context, "select:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty() and not ex.str().empty())
            context.editor().multi_select(std::bind(select_all_matches, _1, _2, ex));
    });
}

void split_regex(Context& context, int)
{
    regex_prompt(context, "split:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty() and not ex.str().empty())
            context.editor().multi_select(std::bind(split_selection, _1, _2, ex));
    });
}

void split_lines(Context& context, int)
{
    context.editor().multi_select([](const Buffer& buffer, const Selection& sel) {
        if (sel.first().line == sel.last().line)
            return SelectionList{ sel };
        SelectionList res;
        auto min = sel.min();
        auto max = sel.max();
        res.push_back({min, {min.line, buffer[min.line].length()-1}});
        for (auto line = min.line+1; line < max.line; ++line)
            res.push_back({line, {line, buffer[line].length()-1}});
        res.push_back({max.line, max});
        return res;
    });
}

void join_select_spaces(Context& context, int)
{
    Editor& editor = context.editor();
    editor.select(select_whole_lines);
    editor.select(select_to_eol, SelectMode::Extend);
    editor.multi_select([](const Buffer& buffer, const Selection& sel)
    {
        SelectionList res = select_all_matches(buffer, sel, Regex{"(\n\\h*)+"});
        // remove last end of line if selected
        kak_assert(std::is_sorted(res.begin(), res.end(),
                   [](const Selection& lhs, const Selection& rhs)
                   { return lhs.min() < rhs.min(); }));
        if (not res.empty() and res.back().max() == buffer.back_coord())
            res.pop_back();
        return res;
    });
    editor.insert(" ", InsertMode::Replace);
}

void join(Context& context, int param)
{
    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    join_select_spaces(context, param);
}

template<bool matching>
void keep(Context& context, int)
{
    constexpr const char* prompt = matching ? "keep matching:" : "keep not matching:";
    regex_prompt(context, prompt, [](const Regex& ex, Context& context) {
        Editor& editor = context.editor();
        const Buffer& buffer = context.buffer();
        SelectionList keep;
        for (auto& sel : editor.selections())
        {
            if (boost::regex_search(buffer.iterator_at(sel.min()),
                                    utf8::next(buffer.iterator_at(sel.max())), ex) == matching)
                keep.push_back(sel);
        }
        if (keep.empty())
            throw runtime_error("no selections remaining");
        editor.select(std::move(keep));
    });
}

void indent(Context& context, int)
{
    size_t width = context.options()["indentwidth"].get<int>();
    String indent(' ', width);

    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    editor.select(select_whole_lines);
    editor.multi_select(std::bind(select_all_matches, _1, _2, Regex{"^[^\n]"}));
    editor.insert(indent, InsertMode::Insert);
}

void deindent(Context& context, int)
{
    int width = context.options()["indentwidth"].get<int>();
    Editor& editor = context.editor();
    DynamicSelectionList sels{editor.buffer(), editor.selections()};
    auto restore_sels = on_scope_end([&]{ editor.select((SelectionList)std::move(sels)); });
    editor.select(select_whole_lines);
    editor.multi_select(std::bind(select_all_matches, _1, _2,
                                  Regex{"^\\h{1," + to_string(width) + "}"}));
    editor.erase();
}

template<ObjectFlags flags>
void select_object(Context& context, int param)
{
    int level = param <= 0 ? 0 : param - 1;
    on_next_key_with_autoinfo(context, [level](Key key, Context& context) {
        if (key.modifiers != Key::Modifiers::None)
            return;
        const Codepoint c = key.key;

        static constexpr struct
        {
            Codepoint key;
            Selection (*func)(const Buffer&, const Selection&, ObjectFlags);
        } selectors[] = {
            { 'w', select_whole_word<Word> },
            { 'W', select_whole_word<WORD> },
            { 's', select_whole_sentence },
            { 'p', select_whole_paragraph },
            { 'i', select_whole_indent },
        };
        for (auto& sel : selectors)
        {
            if (c == sel.key)
                return context.editor().select(std::bind(sel.func, _1, _2, flags));
        }

        static constexpr struct
        {
            CodepointPair pair;
            Codepoint name;
        } surrounding_pairs[] = {
            { { '(', ')' }, 'b' },
            { { '{', '}' }, 'B' },
            { { '[', ']' }, 'r' },
            { { '<', '>' }, '\0' },
            { { '"', '"' }, '\0' },
            { { '\'', '\'' }, '\0' },
        };
        for (auto& sur : surrounding_pairs )
        {
            if (sur.pair.first == c or sur.pair.second == c or
                (sur.name != 0 and sur.name == c))
                return context.editor().select(std::bind(select_surrounding, _1, _2,
                                                         sur.pair, level, flags));
        }
    }, "select object",
    "b,(,):  parenthesis block\n"
    "B,{,}:  braces block     \n"
    "r,[,]:  brackets block   \n"
    "<,>:    angle block      \n"
    "\":    double quote string\n"
    "':    single quote string\n"
    "w:    word               \n"
    "W:    WORD               \n"
    "s:    sentence           \n"
    "p:    paragraph          \n"
    "i:    indent             \n");
}

template<Key::NamedKey key>
void scroll(Context& context, int)
{
    static_assert(key == Key::PageUp or key == Key::PageDown,
                  "scrool only implements PageUp and PageDown");
    Window& window = context.window();
    Buffer& buffer = context.buffer();
    DisplayCoord position = window.position();
    LineCount cursor_line = 0;

    if (key == Key::PageUp)
    {
        position.line -= (window.dimensions().line - 2);
        cursor_line = position.line;
    }
    else if (key == Key::PageDown)
    {
        position.line += (window.dimensions().line - 2);
        cursor_line = position.line + window.dimensions().line - 1;
    }
    auto cursor_pos = utf8::advance(buffer.iterator_at(position.line),
                                    buffer.iterator_at(position.line+1),
                                    position.column);
    window.select(cursor_pos.coord());
    window.set_position(position);
}

void rotate_selections(Context& context, int count)
{
    context.editor().rotate_selections(count != 0 ? count : 1);
}

void rotate_selections_content(Context& context, int count)
{
    if (count == 0)
        count = 1;
    Editor& editor = context.editor();
    auto strings = editor.selections_content();
    count = count % strings.size();
    std::rotate(strings.begin(), strings.end()-count, strings.end());
    editor.insert(strings, InsertMode::Replace);
    editor.rotate_selections(count);
}

enum class SelectFlags
{
    None = 0,
    Reverse = 1,
    Inclusive = 2,
    Extend = 4
};
constexpr SelectFlags operator|(SelectFlags lhs, SelectFlags rhs)
{
    return (SelectFlags)((int) lhs | (int) rhs);
}
constexpr bool operator&(SelectFlags lhs, SelectFlags rhs)
{
    return ((int) lhs & (int) rhs) != 0;
}

template<SelectFlags flags>
void select_to_next_char(Context& context, int param)
{
    on_next_key_with_autoinfo(context, [param](Key key, Context& context) {
        context.editor().select(
            std::bind(flags & SelectFlags::Reverse ? select_to_reverse : select_to,
                      _1, _2, key.key, param, flags & SelectFlags::Inclusive),
            flags & SelectFlags::Extend ? SelectMode::Extend : SelectMode::Replace);
   }, "select to next char","enter char to select to");
}

void start_or_end_macro_recording(Context& context, int)
{
    if (context.client().is_recording())
        context.client().stop_recording();
    else
        on_next_key_with_autoinfo(context, [](Key key, Context& context) {
            if (key.modifiers == Key::Modifiers::None and
                key.key >= 'a' and key.key <= 'z')
                context.client().start_recording(key.key);
        }, "record macro", "enter macro name ");
}

void replay_macro(Context& context, int count)
{
    on_next_key_with_autoinfo(context, [count](Key key, Context& context) mutable {
        if (key.modifiers == Key::Modifiers::None)
        {
            static std::unordered_set<char> running_macros;
            if (contains(running_macros, key.key))
                throw runtime_error("recursive macros call detected");

            memoryview<String> reg_val = RegisterManager::instance()[key.key].values(context);
            if (not reg_val.empty())
            {
                running_macros.insert(key.key);
                auto stop = on_scope_end([&]{ running_macros.erase(key.key); });

                auto keys = parse_keys(reg_val[0]);
                scoped_edition edition(context.editor());
                do { exec_keys(keys, context); } while (--count > 0);
            }
        }
    }, "replay macro", "enter macro name");
}

template<Direction direction>
void jump(Context& context, int)
{
    auto jump = (direction == Forward) ?
                 context.jump_forward() : context.jump_backward();

    Buffer& buffer = const_cast<Buffer&>(jump.buffer());
    BufferManager::instance().set_last_used_buffer(buffer);
    if (&buffer != &context.buffer())
    {
        auto& manager = ClientManager::instance();
        context.change_editor(manager.get_unused_window_for_buffer(buffer));
    }
    context.editor().select(SelectionList{ jump });
}

void save_selections(Context& context, int)
{
    context.push_jump();
    context.print_status({ "saved " + to_string(context.editor().selections().size()) +
                           " selections", get_color("Information") });
}

template<bool insert_at_begin>
void align(Context& context, int)
{
    auto& selections = context.editor().selections();
    auto& buffer = context.buffer();
    auto get_column = [&buffer](BufferCoord coord)
    { return buffer[coord.line].char_count_to(coord.column); };

    std::vector<std::vector<const Selection*>> columns;
    LineCount last_line = -1;
    size_t column = 0;
    for (auto& sel : selections)
    {
        auto line = sel.last().line;
        if (sel.first().line != line)
            throw runtime_error("align cannot work with multi line selections");

        column = (line == last_line) ? column + 1 : 0;
        if (column >= columns.size())
            columns.resize(column+1);
        columns[column].push_back(&sel);
        last_line = line;
    }

    for (auto& col : columns)
    {
        CharCount max_col = 0;
        for (auto& sel : col)
            max_col = std::max(get_column(sel->last()), max_col);
        for (auto& sel : col)
        {
            CharCount padding = max_col - get_column(sel->last());
            auto it = buffer.iterator_at(insert_at_begin ? sel->min() : sel->last());
            buffer.insert(it, String{ ' ', padding });
        }
    }
}

template<typename T>
class Repeated
{
public:
    constexpr Repeated(T t) : m_func(t) {}

    void operator() (Context& context, int count)
    {
        scoped_edition edition(context.editor());
        do { m_func(context, 0); } while(--count > 0);
    }
private:
    T m_func;
};

template<typename T>
constexpr Repeated<T> repeated(T func) { return Repeated<T>(func); }

template<SelectMode mode, typename T>
class Select
{
public:
    constexpr Select(T t) : m_func(t) {}

    void operator() (Context& context, int)
    {
        context.editor().select(m_func, mode);
    }
private:
    T m_func;
};

template<SelectMode mode, typename T>
constexpr Select<mode, T> select(T func) { return Select<mode, T>(func); }

template<typename Type, Direction direction, SelectMode mode = SelectMode::Replace>
void move(Context& context, int count)
{
    Type offset(std::max(count,1));
    context.editor().move_selections(direction == Backward ? -offset : offset, mode);
}

KeyMap keymap =
{
    { { Key::Modifiers::None, 'h' }, move<CharCount, Backward> },
    { { Key::Modifiers::None, 'j' }, move<LineCount, Forward> },
    { { Key::Modifiers::None, 'k' }, move<LineCount, Backward> },
    { { Key::Modifiers::None, 'l' }, move<CharCount, Forward> },

    { { Key::Modifiers::None, 'H' }, move<CharCount, Backward, SelectMode::Extend> },
    { { Key::Modifiers::None, 'J' }, move<LineCount, Forward, SelectMode::Extend> },
    { { Key::Modifiers::None, 'K' }, move<LineCount, Backward, SelectMode::Extend> },
    { { Key::Modifiers::None, 'L' }, move<CharCount, Forward, SelectMode::Extend> },

    { { Key::Modifiers::None, 't' }, select_to_next_char<SelectFlags::None> },
    { { Key::Modifiers::None, 'f' }, select_to_next_char<SelectFlags::Inclusive> },
    { { Key::Modifiers::None, 'T' }, select_to_next_char<SelectFlags::Extend> },
    { { Key::Modifiers::None, 'F' }, select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend> },
    { { Key::Modifiers::Alt,  't' }, select_to_next_char<SelectFlags::Reverse> },
    { { Key::Modifiers::Alt,  'f' }, select_to_next_char<SelectFlags::Inclusive | SelectFlags::Reverse> },
    { { Key::Modifiers::Alt,  'T' }, select_to_next_char<SelectFlags::Extend | SelectFlags::Reverse> },
    { { Key::Modifiers::Alt,  'F' }, select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend | SelectFlags::Reverse> },

    { { Key::Modifiers::None, 'd' }, erase_selections },
    { { Key::Modifiers::None, 'c' }, change },
    { { Key::Modifiers::None, 'i' }, insert<InsertMode::Insert> },
    { { Key::Modifiers::None, 'I' }, insert<InsertMode::InsertAtLineBegin> },
    { { Key::Modifiers::None, 'a' }, insert<InsertMode::Append> },
    { { Key::Modifiers::None, 'A' }, insert<InsertMode::AppendAtLineEnd> },
    { { Key::Modifiers::None, 'o' }, insert<InsertMode::OpenLineBelow> },
    { { Key::Modifiers::None, 'O' }, insert<InsertMode::OpenLineAbove> },
    { { Key::Modifiers::None, 'r' }, replace_with_char },

    { { Key::Modifiers::None, 'g' }, goto_commands<SelectMode::Replace> },
    { { Key::Modifiers::None, 'G' }, goto_commands<SelectMode::Extend> },

    { { Key::Modifiers::None, 'v' }, view_commands },

    { { Key::Modifiers::None, 'y' }, yank },
    { { Key::Modifiers::None, 'Y' }, cat_yank },
    { { Key::Modifiers::None, 'p' }, repeated(paste<InsertMode::Append>) },
    { { Key::Modifiers::None, 'P' }, repeated(paste<InsertMode::Insert>) },
    { { Key::Modifiers::Alt,  'p' }, paste<InsertMode::Replace> },

    { { Key::Modifiers::None, 's' }, select_regex },
    { { Key::Modifiers::None, 'S' }, split_regex },
    { { Key::Modifiers::Alt,  's' }, split_lines },

    { { Key::Modifiers::None, '.' }, repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Context& context, int) { context.editor().clear_selections(); context.editor().select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, command },
    { { Key::Modifiers::None, '|' }, pipe },
    { { Key::Modifiers::None, ' ' }, [](Context& context, int count) { if (count == 0) context.editor().clear_selections();
                                                                       else context.editor().keep_selection(count-1); } },
    { { Key::Modifiers::Alt,  ' ' }, [](Context& context, int count) { if (count == 0) context.editor().flip_selections();
                                                                       else context.editor().remove_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, repeated(select<SelectMode::Replace>(select_to_next_word<Word>)) },
    { { Key::Modifiers::None, 'e' }, repeated(select<SelectMode::Replace>(select_to_next_word_end<Word>)) },
    { { Key::Modifiers::None, 'b' }, repeated(select<SelectMode::Replace>(select_to_previous_word<Word>)) },
    { { Key::Modifiers::None, 'W' }, repeated(select<SelectMode::Extend>(select_to_next_word<Word>)) },
    { { Key::Modifiers::None, 'E' }, repeated(select<SelectMode::Extend>(select_to_next_word_end<Word>)) },
    { { Key::Modifiers::None, 'B' }, repeated(select<SelectMode::Extend>(select_to_previous_word<Word>)) },

    { { Key::Modifiers::Alt,  'w' }, repeated(select<SelectMode::Replace>(select_to_next_word<WORD>)) },
    { { Key::Modifiers::Alt,  'e' }, repeated(select<SelectMode::Replace>(select_to_next_word_end<WORD>)) },
    { { Key::Modifiers::Alt,  'b' }, repeated(select<SelectMode::Replace>(select_to_previous_word<WORD>)) },
    { { Key::Modifiers::Alt,  'W' }, repeated(select<SelectMode::Extend>(select_to_next_word<WORD>)) },
    { { Key::Modifiers::Alt,  'E' }, repeated(select<SelectMode::Extend>(select_to_next_word_end<WORD>)) },
    { { Key::Modifiers::Alt,  'B' }, repeated(select<SelectMode::Extend>(select_to_previous_word<WORD>)) },

    { { Key::Modifiers::Alt,  'l' }, repeated(select<SelectMode::Replace>(select_to_eol)) },
    { { Key::Modifiers::Alt,  'L' }, repeated(select<SelectMode::Extend>(select_to_eol)) },
    { { Key::Modifiers::Alt,  'h' }, repeated(select<SelectMode::Replace>(select_to_eol_reverse)) },
    { { Key::Modifiers::Alt,  'H' }, repeated(select<SelectMode::Extend>(select_to_eol_reverse)) },

    { { Key::Modifiers::None, 'x' }, repeated(select<SelectMode::Replace>(select_line)) },
    { { Key::Modifiers::None, 'X' }, repeated(select<SelectMode::Extend>(select_line)) },
    { { Key::Modifiers::Alt,  'x' }, select<SelectMode::Replace>(select_whole_lines) },
    { { Key::Modifiers::Alt,  'X' }, select<SelectMode::Replace>(trim_partial_lines) },

    { { Key::Modifiers::None, 'm' }, select<SelectMode::Replace>(select_matching) },
    { { Key::Modifiers::None, 'M' }, select<SelectMode::Extend>(select_matching) },

    { { Key::Modifiers::None, '/' }, search<SelectMode::Replace, Forward> },
    { { Key::Modifiers::None, '?' }, search<SelectMode::Extend, Forward> },
    { { Key::Modifiers::Alt,  '/' }, search<SelectMode::Replace, Backward> },
    { { Key::Modifiers::Alt,  '?' }, search<SelectMode::Extend, Backward> },
    { { Key::Modifiers::None, 'n' }, search_next<SelectMode::Replace, Forward> },
    { { Key::Modifiers::Alt,  'n' }, search_next<SelectMode::ReplaceMain, Forward> },
    { { Key::Modifiers::None, 'N' }, search_next<SelectMode::Append, Forward> },
    { { Key::Modifiers::None, '*' }, use_selection_as_search_pattern<true> },
    { { Key::Modifiers::Alt,  '*' }, use_selection_as_search_pattern<false> },

    { { Key::Modifiers::None, 'u' }, repeated([](Context& context, int) { if (not context.editor().undo()) context.print_status({ "nothing left to undo", get_color("Information") }); }) },
    { { Key::Modifiers::None, 'U' }, repeated([](Context& context, int) { if (not context.editor().redo()) context.print_status({ "nothing left to redo", get_color("Information") }); }) },

    { { Key::Modifiers::Alt,  'i' }, select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner> },
    { { Key::Modifiers::Alt,  'a' }, select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd> },
    { { Key::Modifiers::None, ']' }, select_object<ObjectFlags::ToEnd> },
    { { Key::Modifiers::None, '[' }, select_object<ObjectFlags::ToBegin> },

    { { Key::Modifiers::Alt,  'j' }, join },
    { { Key::Modifiers::Alt,  'J' }, join_select_spaces },

    { { Key::Modifiers::Alt,  'k' }, keep<true> },
    { { Key::Modifiers::Alt,  'K' }, keep<false> },

    { { Key::Modifiers::None, '<' }, deindent },
    { { Key::Modifiers::None, '>' }, indent },

    { { Key::Modifiers::Control, 'i' }, jump<Forward> },
    { { Key::Modifiers::Control, 'o' }, jump<Backward> },
    { { Key::Modifiers::Control, 's' }, save_selections },

    { { Key::Modifiers::Alt,  'r' }, rotate_selections },
    { { Key::Modifiers::Alt,  'R' }, rotate_selections_content },

    { { Key::Modifiers::None, 'q' }, start_or_end_macro_recording },
    { { Key::Modifiers::None, 'Q' }, replay_macro },

    { { Key::Modifiers::None, '`' }, for_each_char<to_lower> },
    { { Key::Modifiers::None, '~' }, for_each_char<to_upper> },
    { { Key::Modifiers::Alt,  '`' },  for_each_char<swap_case> },

    { { Key::Modifiers::None, '&' }, align<false> },
    { { Key::Modifiers::Alt,  '&' }, align<true> },

    { Key::Left,  move<CharCount, Backward> },
    { Key::Down,  move<LineCount, Forward> },
    { Key::Up,    move<LineCount, Backward> },
    { Key::Right, move<CharCount, Forward> },

    { Key::PageUp,   scroll<Key::PageUp> },
    { Key::PageDown, scroll<Key::PageDown> },
};

}
