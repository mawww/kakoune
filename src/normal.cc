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
#include "debug.hh"

namespace Kakoune
{

void erase(Buffer& buffer, SelectionList& selections)
{
    for (auto& sel : selections)
    {
        erase(buffer, sel);
        selections.update();
        avoid_eol(buffer, sel);
    }
    selections.check_invariant();
    buffer.check_invariant();
}

template<InsertMode mode>
BufferIterator prepare_insert(Buffer& buffer, const Selection& sel)
{
    switch (mode)
    {
    case InsertMode::Insert:
        return buffer.iterator_at(sel.min());
    case InsertMode::Replace:
        return Kakoune::erase(buffer, sel);
    case InsertMode::Append:
    {
        // special case for end of lines, append to current line instead
        auto pos = buffer.iterator_at(sel.max());
        return *pos == '\n' ? pos : utf8::next(pos);
    }
    case InsertMode::InsertAtLineBegin:
        return buffer.iterator_at(sel.min().line);
    case InsertMode::AppendAtLineEnd:
        return buffer.iterator_at({sel.max().line, buffer[sel.max().line].length() - 1});
    case InsertMode::InsertAtNextLineBegin:
        return buffer.iterator_at(sel.max().line+1);
    case InsertMode::OpenLineBelow:
        return buffer.insert(buffer.iterator_at(sel.max().line + 1), "\n");
    case InsertMode::OpenLineAbove:
        return buffer.insert(buffer.iterator_at(sel.min().line), "\n");
    }
    kak_assert(false);
    return {};
}

template<InsertMode mode>
void insert(Buffer& buffer, SelectionList& selections, const String& str)
{
    for (auto& sel : selections)
    {
        auto pos = prepare_insert<mode>(buffer, sel);
        pos = buffer.insert(pos, str);
        selections.update();
        if (mode == InsertMode::Replace and pos != buffer.end())
        {
            sel.anchor() = pos.coord();
            sel.cursor() = str.empty() ?
                pos.coord() : (pos + str.byte_count_to(str.char_length() - 1)).coord();
        }
        avoid_eol(buffer, sel);
    }
    selections.check_invariant();
    buffer.check_invariant();
}

template<InsertMode mode>
void insert(Buffer& buffer, SelectionList& selections, memoryview<String> strings)
{
    if (strings.empty())
        return;
    for (size_t i = 0; i < selections.size(); ++i)
    {
        auto& sel = selections[i];
        auto pos = prepare_insert<mode>(buffer, sel);
        const String& str = strings[std::min(i, strings.size()-1)];
        pos = buffer.insert(pos, str);
        selections.update();
        if (mode == InsertMode::Replace and pos != buffer.end())
        {
            sel.anchor() = pos.coord();
            sel.cursor() = (str.empty() ?
                pos : pos + str.byte_count_to(str.char_length() - 1)).coord();
        }
        avoid_eol(buffer, sel);
    }
    selections.check_invariant();
    buffer.check_invariant();
}

using namespace std::placeholders;

enum class SelectMode
{
    Replace,
    Extend,
    Append,
    ReplaceMain,
};

template<SelectMode mode = SelectMode::Replace, typename Func>
void select(Context& context, Func func)
{
    auto& buffer = context.buffer();
    auto& selections = context.selections();
    if (mode == SelectMode::Append)
    {
        auto& sel = selections.main();
        auto  res = func(buffer, sel);
        if (res.captures().empty())
            res.captures() = sel.captures();
        selections.push_back(res);
        selections.set_main_index(selections.size() - 1);
    }
    else if (mode == SelectMode::ReplaceMain)
    {
        auto& sel = selections.main();
        auto  res = func(buffer, sel);
        sel.anchor() = res.anchor();
        sel.cursor() = res.cursor();
        if (not res.captures().empty())
            sel.captures() = std::move(res.captures());
    }
    else
    {
        for (auto& sel : selections)
        {
            auto res = func(buffer, sel);
            if (mode == SelectMode::Extend)
                sel.merge_with(res);
            else
            {
                sel.anchor() = res.anchor();
                sel.cursor() = res.cursor();
            }
            if (not res.captures().empty())
                sel.captures() = std::move(res.captures());
        }
    }
    selections.sort_and_merge_overlapping();
    selections.check_invariant();
}

template<SelectMode mode, typename T>
class Select
{
public:
    constexpr Select(T t) : m_func(t) {}
    void operator() (Context& context, int) { select<mode>(context, m_func); }
private:
    T m_func;
};

template<SelectMode mode = SelectMode::Replace, typename T>
constexpr Select<mode, T> make_select(T func)
{
    return Select<mode, T>(func);
}

template<SelectMode mode = SelectMode::Replace>
void select_coord(const Buffer& buffer, ByteCoord coord, SelectionList& selections)
{
    coord = buffer.clamp(coord);
    if (mode == SelectMode::Replace)
        selections = SelectionList{ buffer, coord };
    else if (mode == SelectMode::Extend)
    {
        for (auto& sel : selections)
            sel.cursor() = coord;
        selections.sort_and_merge_overlapping();
    }
}

template<InsertMode mode>
void enter_insert_mode(Context& context, int)
{
    context.input_handler().insert(mode);
}

void repeat_last_insert(Context& context, int)
{
    context.input_handler().repeat_last_insert();
}

bool show_auto_info_ifn(const String& title, const String& info,
                        const Context& context)
{
    if (not context.options()["autoinfo"].get<bool>() or not context.has_ui())
        return false;
    ColorPair col = get_color("Information");
    CharCoord pos = context.window().dimensions();
    pos.column -= 1;
    context.ui().info_show(title, info, pos , col, MenuStyle::Prompt);
    return true;
}

template<typename Cmd>
void on_next_key_with_autoinfo(const Context& context, Cmd cmd,
                               const String& title, const String& info)
{
    const bool hide = show_auto_info_ifn(title, info, context);
    context.input_handler().on_next_key([hide,cmd](Key key, Context& context) mutable {
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
        select_coord<mode>(context.buffer(), LineCount{line - 1}, context.selections());
        if (context.has_window())
            context.window().center_line(LineCount{line-1});
    }
    else
    {
        on_next_key_with_autoinfo(context, [](Key key, Context& context) {
            if (key.modifiers != Key::Modifiers::None)
                return;
            auto& buffer = context.buffer();
            switch (tolower(key.key))
            {
            case 'g':
            case 'k':
                context.push_jump();
                select_coord<mode>(buffer, ByteCoord{0,0}, context.selections());
                break;
            case 'l':
                select<mode>(context, select_to_eol);
                break;
            case 'h':
                select<mode>(context, select_to_eol_reverse);
                break;
            case 'j':
            {
                context.push_jump();
                select_coord<mode>(buffer, buffer.line_count() - 1, context.selections());
                break;
            }
            case 'e':
                context.push_jump();
                select_coord<mode>(buffer, buffer.back_coord(), context.selections());
                break;
            case 't':
                if (context.has_window())
                {
                    auto line = context.window().position().line;
                    select_coord<mode>(buffer, line, context.selections());
                }
                break;
            case 'b':
                if (context.has_window())
                {
                    auto& window = context.window();
                    auto line = window.position().line + window.dimensions().line - 1;
                    select_coord<mode>(buffer, line, context.selections());
                }
                break;
            case 'c':
                if (context.has_window())
                {
                    auto& window = context.window();
                    auto line = window.position().line + window.dimensions().line / 2;
                    select_coord<mode>(buffer, line, context.selections());
                }
                break;
            case 'a':
            {
                auto& buffer_manager = BufferManager::instance();
                auto it = buffer_manager.begin();
                if (it->get() == &buffer and ++it == buffer_manager.end())
                    break;
                context.push_jump();
                context.change_buffer(**it);
                break;
            }
            case 'f':
            {
                const Selection& sel = context.selections().main();
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

                Buffer* buffer = create_buffer_from_file(path);
                if (buffer == nullptr)
                    throw runtime_error("unable to open file '" + path + "'");

                if (buffer != &context.buffer())
                {
                    BufferManager::instance().set_last_used_buffer(*buffer);
                    context.push_jump();
                    context.change_buffer(*buffer);
                }
                break;
            }
            case '.':
            {
                context.push_jump();
                auto pos = buffer.last_modification_coord();
                if (buffer[pos.line].length() == pos.column + 1)
                    pos = ByteCoord{ pos.line+1, 0 };
                select_coord<mode>(buffer, pos, context.selections());
                break;
            }
            }
        }, "goto",
        "g,k:  buffer top        \n"
        "l:    line end          \n"
        "h:    line begin        \n"
        "j:    buffer bottom     \n"
        "e:    buffer end        \n"
        "t:    window top        \n"
        "b:    window bottom     \n"
        "c:    window center     \n"
        "a:    last buffer       \n"
        "f:    file              \n"
        ".:    last buffer change\n");
    }
}

void view_commands(Context& context, int param)
{
    on_next_key_with_autoinfo(context, [param](Key key, Context& context) {
        if (key.modifiers != Key::Modifiers::None or not context.has_window())
            return;

        LineCount cursor_line = context.selections().main().cursor().line;
        Window& window = context.window();
        switch (tolower(key.key))
        {
        case 'v':
        case 'c':
            context.window().center_line(cursor_line);
            break;
        case 't':
            context.window().display_line_at(cursor_line, 0);
            break;
        case 'b':
            context.window().display_line_at(cursor_line, window.dimensions().line-1);
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
        if (not iswprint(key.key))
            return;
        ScopedEdition edition(context);
        Buffer& buffer = context.buffer();
        SelectionList& selections = context.selections();
        std::vector<String> strings;
        for (auto& sel : selections)
        {
            CharCount count = char_length(buffer, sel);
            strings.emplace_back(key.key, count);
        }
        insert<InsertMode::Replace>(buffer, selections, strings);
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
    ScopedEdition edition(context);
    std::vector<String> sels = context.selections_content();
    for (auto& sel : sels)
    {
        for (auto& c : sel)
            c = func(c);
    }
    insert<InsertMode::Replace>(context.buffer(), context.selections(), sels);
}

void command(Context& context, int)
{
    context.input_handler().prompt(
        ":", "", get_color("Prompt"),
        std::bind(&CommandManager::complete, &CommandManager::instance(), _1, _2, _3, _4),
        [](const String& cmdline, PromptEvent event, Context& context) {
            if (context.has_ui())
            {
                context.ui().info_hide();
                if (event == PromptEvent::Change and context.options()["autoinfo"].get<bool>())
                {
                    auto info = CommandManager::instance().command_info(cmdline);
                    ColorPair col = get_color("Information");
                    CharCoord pos = context.window().dimensions();
                    pos.column -= 1;
                    if (not info.first.empty() and not info.second.empty())
                        context.ui().info_show(info.first, info.second, pos , col, MenuStyle::Prompt);
                }
            }
            if (event == PromptEvent::Validate)
                CommandManager::instance().execute(cmdline, context);
        });
}

template<InsertMode mode>
void pipe(Context& context, int)
{
    const char* prompt = mode == InsertMode::Replace ? "pipe:" : "pipe (ins):";
    context.input_handler().prompt(prompt, "", get_color("Prompt"), shell_complete,
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

            Buffer& buffer = context.buffer();
            SelectionList& selections = context.selections();
            std::vector<String> strings;
            for (auto& sel : selections)
            {
                auto str = content(buffer, sel);
                bool insert_eol = str.back() != '\n';
                if (insert_eol)
                    str += '\n';
                str = ShellManager::instance().pipe(str, real_cmd, context,
                                                    {}, EnvVarMap{});
                if (insert_eol and str.back() == '\n')
                    str = str.substr(0, str.length()-1);
                strings.push_back(str);
            }
            ScopedEdition edition(context);
            insert<mode>(buffer, selections, strings);
        });
}

template<Direction direction, SelectMode mode>
void select_next_match(const Buffer& buffer, SelectionList& selections,
                       const Regex& regex)
{
    if (mode == SelectMode::Replace)
    {
        for (auto& sel : selections)
            sel = find_next_match<direction>(buffer, sel, regex);
    }
    if (mode == SelectMode::Extend)
    {
        for (auto& sel : selections)
            sel.merge_with(find_next_match<direction>(buffer, sel, regex));
    }
    else if (mode == SelectMode::ReplaceMain)
        selections.main() = find_next_match<direction>(buffer, selections.main(), regex);
    else if (mode == SelectMode::Append)
    {
        selections.push_back(find_next_match<direction>(buffer, selections.main(), regex));
        selections.set_main_index(selections.size() - 1);
    }
    selections.sort_and_merge_overlapping();
}

void yank(Context& context, int)
{
    RegisterManager::instance()['"'] = context.selections_content();
    context.print_status({ "yanked " + to_string(context.selections().size()) +
                           " selections", get_color("Information") });
}

void cat_yank(Context& context, int)
{
    auto sels = context.selections_content();
    String str;
    for (auto& sel : sels)
        str += sel;
    RegisterManager::instance()['"'] = memoryview<String>(str);
    context.print_status({ "concatenated and yanked " +
                           to_string(sels.size()) + " selections", get_color("Information") });
}

void erase_selections(Context& context, int)
{
    RegisterManager::instance()['"'] = context.selections_content();
    ScopedEdition edition(context);
    erase(context.buffer(), context.selections());
}

void cat_erase_selections(Context& context, int)
{
    auto sels = context.selections_content();
    String str;
    for (auto& sel : sels)
        str += sel;
    RegisterManager::instance()['"'] = memoryview<String>(str);
    erase(context.buffer(), context.selections());
}


void change(Context& context, int param)
{
    RegisterManager::instance()['"'] = context.selections_content();
    enter_insert_mode<InsertMode::Replace>(context, param);
}

constexpr InsertMode adapt_for_linewise(InsertMode mode)
{
    return ((mode == InsertMode::Append) ?
             InsertMode::InsertAtNextLineBegin :
             ((mode == InsertMode::Insert) ?
               InsertMode::InsertAtLineBegin :
               ((mode == InsertMode::Replace) ?
                 InsertMode::Replace : InsertMode::Insert)));
}

template<InsertMode mode>
void paste(Context& context, int)
{
    auto strings = RegisterManager::instance()['"'].values(context);
    bool linewise = false;
    for (auto& str : strings)
    {
        if (not str.empty() and str.back() == '\n')
        {
            linewise = true;
            break;
        }
    }
    ScopedEdition edition(context);
    if (linewise)
        insert<adapt_for_linewise(mode)>(context.buffer(), context.selections(), strings);
    else
        insert<mode>(context.buffer(), context.selections(), strings);
}

template<typename T>
void regex_prompt(Context& context, const String prompt, T func)
{
    SelectionList selections = context.selections();
    context.input_handler().prompt(prompt, "", get_color("Prompt"), complete_nothing,
        [=](const String& str, PromptEvent event, Context& context) mutable {
            try
            {
                if (event != PromptEvent::Change and context.has_ui())
                    context.ui().info_hide();
                selections.update();
                context.selections() = selections;
                context.input_handler().set_prompt_colors(get_color("Prompt"));
                if (event == PromptEvent::Abort)
                    return;
                if (event == PromptEvent::Change and
                    (str.empty() or not context.options()["incsearch"].get<bool>()))
                    return;

                if (event == PromptEvent::Validate)
                    context.push_jump();
                func(str.empty() ? Regex{} : Regex{str}, context);
            }
            catch (boost::regex_error& err)
            {
                if (event == PromptEvent::Validate)
                    throw runtime_error("regex error: "_str + err.what());
                else
                    context.input_handler().set_prompt_colors(get_color("Error"));
            }
            catch (std::runtime_error& err)
            {
                if (event == PromptEvent::Validate)
                    throw runtime_error("regex error: "_str + err.what());
                else
                {
                    context.input_handler().set_prompt_colors(get_color("Error"));
                    if (context.has_ui())
                    {
                        ColorPair col = get_color("Information");
                        CharCoord pos = context.window().dimensions();
                        pos.column -= 1;
                        context.ui().info_show("regex error", err.what(), pos, col, MenuStyle::Prompt);
                    }
                }
            }
            catch (runtime_error&)
            {
                context.selections() = selections;
                // only validation should propagate errors,
                // incremental search should not.
                if (event == PromptEvent::Validate)
                    throw;
            }
        });
}

template<SelectMode mode, Direction direction>
void search(Context& context, int)
{
    regex_prompt(context, direction == Forward ? "search:" : "reverse search:",
                 [](Regex ex, Context& context) {
                     if (ex.empty())
                         ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
                     else
                         RegisterManager::instance()['/'] = String{ex.str()};
                     if (not ex.empty() and not ex.str().empty())
                         select_next_match<direction, mode>(context.buffer(), context.selections(), ex);
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
                select_next_match<direction, mode>(context.buffer(), context.selections(), ex);
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
    auto& sels = context.selections();
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

void select_regex(Context& context, int)
{
    regex_prompt(context, "select:", [](Regex ex, Context& context) {
        if (ex.empty())
            ex = Regex{RegisterManager::instance()['/'].values(context)[0]};
        else
            RegisterManager::instance()['/'] = String{ex.str()};
        if (not ex.empty() and not ex.str().empty())
            select_all_matches(context.buffer(), context.selections(), ex);
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
            split_selections(context.buffer(), context.selections(), ex);
    });
}

void split_lines(Context& context, int)
{
    auto& selections = context.selections();
    auto& buffer = context.buffer();
    std::vector<Selection> res;
    for (auto& sel : selections)
    {
        if (sel.anchor().line == sel.cursor().line)
        {
             res.push_back(std::move(sel));
             continue;
        }
        auto min = sel.min();
        auto max = sel.max();
        res.push_back({min, {min.line, buffer[min.line].length()-1}});
        for (auto line = min.line+1; line < max.line; ++line)
            res.push_back({line, {line, buffer[line].length()-1}});
        res.push_back({max.line, max});
    }
    selections = std::move(res);
}

void join_select_spaces(Context& context, int)
{
    auto& buffer = context.buffer();
    std::vector<Selection> selections;
    for (auto& sel : context.selections())
    {
        for (LineCount line = sel.min().line; line <= sel.max().line; ++line)
        {
            if (line == buffer.line_count() - 1)
                continue;
            auto begin = buffer.iterator_at({line, buffer[line].length()-1});
            auto end = begin+1;
            skip_while(end, buffer.end(), is_horizontal_blank);
            selections.push_back({begin.coord(), (end-1).coord()});
        }
    }
    if (selections.empty())
        return;
    context.selections() = selections;
    ScopedEdition edition(context);
    insert<InsertMode::Replace>(buffer, context.selections(), " ");
}

void join(Context& context, int param)
{
    SelectionList sels{context.selections()};
    auto restore_sels = on_scope_end([&]{
        sels.update();
        context.selections() = std::move(sels);
    });

    join_select_spaces(context, param);
}

template<bool matching>
void keep(Context& context, int)
{
    constexpr const char* prompt = matching ? "keep matching:" : "keep not matching:";
    regex_prompt(context, prompt, [](const Regex& ex, Context& context) {
        if (ex.empty())
            return;
        const Buffer& buffer = context.buffer();
        std::vector<Selection> keep;
        for (auto& sel : context.selections())
        {
            if (boost::regex_search(buffer.iterator_at(sel.min()),
                                    utf8::next(buffer.iterator_at(sel.max())), ex) == matching)
                keep.push_back(sel);
        }
        if (keep.empty())
            throw runtime_error("no selections remaining");
        context.selections() = std::move(keep);
    });
}

void keep_pipe(Context& context, int)
{
    context.input_handler().prompt(
        "keep pipe:", "", get_color("Prompt"), shell_complete,
        [](const String& cmdline, PromptEvent event, Context& context) {
            if (event != PromptEvent::Validate)
                return;
            const Buffer& buffer = context.buffer();
            auto& shell_manager = ShellManager::instance();
            std::vector<Selection> keep;
            for (auto& sel : context.selections())
            {
                int status = 0;
                shell_manager.pipe(content(buffer, sel), cmdline, context,
                                   {}, EnvVarMap{}, &status);
                if (status == 0)
                    keep.push_back(sel);
            }
            if (keep.empty())
                throw runtime_error("no selections remaining");
            context.selections() = std::move(keep);
    });
}
template<bool indent_empty = false>
void indent(Context& context, int)
{
    CharCount indent_width = context.options()["indentwidth"].get<int>();
    String indent = indent_width == 0 ? "\t" : String{' ', indent_width};

    auto& buffer = context.buffer();
    std::vector<Selection> sels;
    for (auto& sel : context.selections())
    {
        for (auto line = sel.min().line; line < sel.max().line+1; ++line)
        {
            if (indent_empty or buffer[line].length() > 1)
                sels.push_back({line, line});
        }
    }
    if (not sels.empty())
    {
        ScopedEdition edition(context);
        SelectionList selections{buffer, std::move(sels)};
        insert<InsertMode::Insert>(buffer, selections, indent);
    }
}

template<bool deindent_incomplete = true>
void deindent(Context& context, int)
{
    CharCount tabstop = context.options()["tabstop"].get<int>();
    CharCount indent_width = context.options()["indentwidth"].get<int>();
    if (indent_width == 0)
        indent_width = tabstop;

    auto& buffer = context.buffer();
    std::vector<Selection> sels;
    for (auto& sel : context.selections())
    {
        for (auto line = sel.min().line; line < sel.max().line+1; ++line)
        {
            CharCount width = 0;
            auto& content = buffer[line];
            for (auto column = 0_byte; column < content.length(); ++column)
            {
                const char c = content[column];
                if (c == '\t')
                    width = (width / tabstop + 1) * tabstop;
                else if (c == ' ')
                    ++width;
                else
                {
                    if (deindent_incomplete and width != 0)
                        sels.push_back({ line, ByteCoord{line, column-1} });
                    break;
                }
                if (width == indent_width)
                {
                    sels.push_back({ line, ByteCoord{line, column} });
                    break;
                }
            }
        }
    }
    if (not sels.empty())
    {
        ScopedEdition edition(context);
        SelectionList selections{context.buffer(), std::move(sels)};
        erase(context.buffer(), selections);
    }
}

template<ObjectFlags flags, SelectMode mode = SelectMode::Replace>
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
                return select<mode>(context, std::bind(sel.func, _1, _2, flags));
        }

        static const struct
        {
            CodepointPair pair;
            Codepoint name;
        } surrounding_pairs[] = {
            { { '(', ')' }, 'b' },
            { { '{', '}' }, 'B' },
            { { '[', ']' }, 'r' },
            { { '<', '>' }, 'a' },
            { { '"', '"' }, 'Q' },
            { { '\'', '\'' }, 'q' },
            { { '`', '`' }, 'g' },
        };
        for (auto& sur : surrounding_pairs)
        {
            if (sur.pair.first == c or sur.pair.second == c or
                (sur.name != 0 and sur.name == c))
                return select<mode>(context, std::bind(select_surrounding, _1, _2,
                                                       sur.pair, level, flags));
        }
    }, "select object",
    "b,(,):  parenthesis block\n"
    "B,{,}:  braces block     \n"
    "r,[,]:  brackets block   \n"
    "a,<,>:  angle block      \n"
    "\",Q:  double quote string\n"
    "',q:  single quote string\n"
    "`,g:  grave quote string \n"
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
    CharCoord position = window.position();
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
    select_coord(buffer, cursor_pos.coord(), context.selections());
    window.set_position(position);
}

void rotate_selections(Context& context, int count)
{
    context.selections().rotate_main(count != 0 ? count : 1);
}

void rotate_selections_content(Context& context, int group)
{
    int count = 1;
    auto strings = context.selections_content();
    if (group == 0 or group > (int)strings.size())
        group = (int)strings.size();
    count = count % group;
    for (auto it = strings.begin(); it != strings.end(); )
    {
        auto end = std::min(strings.end(), it + group);
        std::rotate(it, end-count, end);
        it = end;
    }
    insert<InsertMode::Replace>(context.buffer(), context.selections(), strings);
    context.selections().rotate_main(count);
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
        select<flags & SelectFlags::Extend ? SelectMode::Extend : SelectMode::Replace>(
            context,
            std::bind(flags & SelectFlags::Reverse ? select_to_reverse : select_to,
                      _1, _2, key.key, param, flags & SelectFlags::Inclusive));
    }, "select to next char","enter char to select to");
}

void start_or_end_macro_recording(Context& context, int)
{
    if (context.input_handler().is_recording())
        context.input_handler().stop_recording();
    else
        on_next_key_with_autoinfo(context, [](Key key, Context& context) {
            if (key.modifiers == Key::Modifiers::None and
                key.key >= 'a' and key.key <= 'z')
                context.input_handler().start_recording(key.key);
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
                ScopedEdition edition(context);
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
        context.change_buffer(buffer);
    context.selections() = jump;
}

void save_selections(Context& context, int)
{
    context.push_jump();
    context.print_status({ "saved " + to_string(context.selections().size()) +
                           " selections", get_color("Information") });
}

void align(Context& context, int)
{
    auto& selections = context.selections();
    auto& buffer = context.buffer();
    const CharCount tabstop = context.options()["tabstop"].get<int>();

    std::vector<std::vector<const Selection*>> columns;
    LineCount last_line = -1;
    size_t column = 0;
    for (auto& sel : selections)
    {
        auto line = sel.cursor().line;
        if (sel.anchor().line != line)
            throw runtime_error("align cannot work with multi line selections");

        column = (line == last_line) ? column + 1 : 0;
        if (column >= columns.size())
            columns.resize(column+1);
        columns[column].push_back(&sel);
        last_line = line;
    }

    const bool use_tabs = context.options()["aligntab"].get<bool>();
    for (auto& col : columns)
    {
        CharCount maxcol = 0;
        for (auto& sel : col)
            maxcol = std::max(get_column(buffer, tabstop, sel->cursor()), maxcol);
        for (auto& sel : col)
        {
            auto insert_coord = sel->min();
            auto lastcol = get_column(buffer, tabstop, sel->cursor());
            String padstr;
            if (not use_tabs)
                padstr = String{ ' ', maxcol - lastcol };
            else
            {
                auto inscol = get_column(buffer, tabstop, insert_coord);
                auto targetcol = maxcol - (lastcol - inscol);
                auto tabcol = inscol - (inscol % tabstop);
                auto tabs = (targetcol - tabcol) / tabstop;
                auto spaces = targetcol - (tabs ? (tabcol + tabs * tabstop) : inscol);
                padstr = String{ '\t', tabs } + String{ ' ', spaces };
            }
            buffer.insert(buffer.iterator_at(insert_coord), std::move(padstr));
        }
    }
}

void copy_indent(Context& context, int selection)
{
    auto& buffer = context.buffer();
    auto& selections = context.selections();
    std::vector<LineCount> lines;
    for (auto sel : selections)
    {
        for (LineCount l = sel.min().line; l < sel.max().line + 1; ++l)
            lines.push_back(l);
    }
    if (selection > selections.size())
        throw runtime_error("invalid selection index");
    if (selection == 0)
        selection = context.selections().main_index() + 1;

    const String& line = buffer[selections[selection-1].min().line];
    auto it = line.begin();
    while (it != line.end() and is_horizontal_blank(*it))
        ++it;
    const String indent{line.begin(), it};

    ScopedEdition edition{context};
    for (auto& l : lines)
    {
        auto& line = buffer[l];
        ByteCount i = 0;
        while (i < line.length() and is_horizontal_blank(line[i]))
            ++i;
        buffer.erase(buffer.iterator_at(l), buffer.iterator_at({l, i}));
        buffer.insert(buffer.iterator_at(l), indent);
    }
}

void tabs_to_spaces(Context& context, int ts)
{
    auto& buffer = context.buffer();
    const CharCount opt_tabstop = context.options()["tabstop"].get<int>();
    const CharCount tabstop = ts == 0 ? opt_tabstop : ts;
    for (auto& sel : context.selections())
    {
        for (auto it = buffer.iterator_at(sel.min()),
                  end = buffer.iterator_at(sel.max())+1; it != end;)
        {
            if (*it == '\t')
            {
                CharCount col = get_column(buffer, opt_tabstop, it.coord());
                CharCount end_col = (col / tabstop + 1) * tabstop;
                it = buffer.erase(it, it+1);
                it = buffer.insert(it, String{ ' ', end_col - col }) + (int)(end_col - col);
                end = buffer.iterator_at(sel.max())+1;
            }
            else
                ++it;
        }
    }
}

void spaces_to_tabs(Context& context, int ts)
{
    auto& buffer = context.buffer();
    const CharCount opt_tabstop = context.options()["tabstop"].get<int>();
    const CharCount tabstop = ts == 0 ? opt_tabstop : ts;
    for (auto& sel : context.selections())
    {
        for (auto it = buffer.iterator_at(sel.min()),
                  end = buffer.iterator_at(sel.max())+1; it != end;)
        {
            if (*it == ' ')
            {
                auto spaces_beg = it;
                auto spaces_end = spaces_beg+1;
                CharCount col = get_column(buffer, opt_tabstop, spaces_end.coord());
                while (*spaces_end == ' ' and (col % tabstop) != 0)
                {
                    ++spaces_end;
                    ++col;
                }

                if ((col % tabstop) == 0)
                {
                    it = buffer.erase(spaces_beg, spaces_end);
                    it = buffer.insert(it, "\t") + 1;
                    end = buffer.iterator_at(sel.max())+1;
                }
                else
                    it = spaces_end;
            }
            else
                ++it;
        }
    }
}

static boost::optional<SelectionList> compute_modified_ranges(const Buffer& buffer, size_t timestamp)
{
    std::vector<Selection> ranges;
    for (auto& change : buffer.changes_since(timestamp))
    {
        const ByteCoord& begin = change.begin;
        const ByteCoord& end = change.end;
        if (change.type == Buffer::Change::Insert)
        {
            update_insert(ranges, begin, end, change.at_end);
            auto it = std::upper_bound(ranges.begin(), ranges.end(), begin,
                                       [](ByteCoord c, const Selection& sel)
                                       { return c < sel.min(); });
            ranges.insert(it, Selection{begin, end});
        }
        else
        {
            update_erase(ranges, begin, end, change.at_end);
            auto pos = begin;
            if (change.at_end)
                pos = begin.column ? ByteCoord{begin.line, begin.column - 1}
                                   : ByteCoord{begin.line - 1};
            auto it = std::upper_bound(ranges.begin(), ranges.end(), pos,
                                       [](ByteCoord c, const Selection& sel)
                                       { return c < sel.min(); });
            ranges.insert(it, Selection{pos, pos});
        }
    }
    if (ranges.empty())
        return {};

    SelectionList result{buffer, std::move(ranges)};

    auto touches = [&](const Selection& lhs, const Selection& rhs) {
        return lhs.min() <= rhs.min() ? buffer.char_next(lhs.max()) >= rhs.min()
                                      : lhs.min() <= buffer.char_next(rhs.max());
    };
    result.merge_overlapping(touches);

    for (auto& sel : result)
    {
        if (sel.anchor() != sel.cursor())
            sel.cursor() = buffer.char_prev(sel.cursor());
    }
    return result;
}

void undo(Context& context, int)
{
    Buffer& buffer = context.buffer();
    size_t timestamp = buffer.timestamp();
    bool res = buffer.undo();
    if (res)
    {
        auto ranges = compute_modified_ranges(buffer, timestamp);
        if (ranges)
            context.selections() = std::move(*ranges);
    }
    else if (not res)
        context.print_status({ "nothing left to undo", get_color("Information") });
}

void redo(Context& context, int)
{
    using namespace std::placeholders;
    Buffer& buffer = context.buffer();
    size_t timestamp = buffer.timestamp();
    bool res = buffer.redo();
    if (res)
    {
        auto ranges = compute_modified_ranges(buffer, timestamp);
        if (ranges)
            context.selections() = std::move(*ranges);
    }

    else if (not res)
        context.print_status({ "nothing left to redo", get_color("Information") });
}

template<typename T>
class Repeated
{
public:
    constexpr Repeated(T t) : m_func(t) {}

    void operator() (Context& context, int count)
    {
        ScopedEdition edition(context);
        do { m_func(context, 0); } while(--count > 0);
    }
private:
    T m_func;
};

template<typename T>
constexpr Repeated<T> repeated(T func) { return Repeated<T>(func); }

template<typename Type, Direction direction, SelectMode mode = SelectMode::Replace>
void move(Context& context, int count)
{
    kak_assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    Type offset(std::max(count,1));
    if (direction == Backward)
        offset = -offset;
    auto& selections = context.selections();
    for (auto& sel : selections)
    {
        auto cursor = context.has_window() ? context.window().offset_coord(sel.cursor(), offset)
                                           : context.buffer().offset_coord(sel.cursor(), offset);

        sel.anchor() = mode == SelectMode::Extend ? sel.anchor() : cursor;
        sel.cursor()  = cursor;
        avoid_eol(context.buffer(), sel);
    }
    selections.sort_and_merge_overlapping();
}

KeyMap keymap =
{
    { 'h', move<CharCount, Backward> },
    { 'j', move<LineCount, Forward> },
    { 'k', move<LineCount, Backward> },
    { 'l', move<CharCount, Forward> },

    { 'H', move<CharCount, Backward, SelectMode::Extend> },
    { 'J', move<LineCount, Forward, SelectMode::Extend> },
    { 'K', move<LineCount, Backward, SelectMode::Extend> },
    { 'L', move<CharCount, Forward, SelectMode::Extend> },

    { 't', select_to_next_char<SelectFlags::None> },
    { 'f', select_to_next_char<SelectFlags::Inclusive> },
    { 'T', select_to_next_char<SelectFlags::Extend> },
    { 'F', select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend> },
    { alt('t'), select_to_next_char<SelectFlags::Reverse> },
    { alt('f'), select_to_next_char<SelectFlags::Inclusive | SelectFlags::Reverse> },
    { alt('T'), select_to_next_char<SelectFlags::Extend | SelectFlags::Reverse> },
    { alt('F'), select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend | SelectFlags::Reverse> },

    { 'd', erase_selections },
    { 'D', cat_erase_selections },
    { 'c', change },
    { 'i', enter_insert_mode<InsertMode::Insert> },
    { 'I', enter_insert_mode<InsertMode::InsertAtLineBegin> },
    { 'a', enter_insert_mode<InsertMode::Append> },
    { 'A', enter_insert_mode<InsertMode::AppendAtLineEnd> },
    { 'o', enter_insert_mode<InsertMode::OpenLineBelow> },
    { 'O', enter_insert_mode<InsertMode::OpenLineAbove> },
    { 'r', replace_with_char },

    { 'g', goto_commands<SelectMode::Replace> },
    { 'G', goto_commands<SelectMode::Extend> },

    { 'v', view_commands },

    { 'y', yank },
    { 'Y', cat_yank },
    { 'p', repeated(paste<InsertMode::Append>) },
    { 'P', repeated(paste<InsertMode::Insert>) },
    { alt('p'), paste<InsertMode::Replace> },

    { 's', select_regex },
    { 'S', split_regex },
    { alt('s'), split_lines },

    { '.', repeat_last_insert },

    { '%', [](Context& context, int) { select_whole_buffer(context.buffer(), context.selections()); } },

    { ':', command },
    { '|', pipe<InsertMode::Replace> },
    { alt('|'), pipe<InsertMode::Append> },
    { ' ', [](Context& context, int count) { if (count == 0) clear_selections(context.buffer(), context.selections());
                                             else keep_selection(context.selections(), count-1); } },
    { alt(' '), [](Context& context, int count) { if (count == 0) flip_selections(context.selections());
                                                  else remove_selection(context.selections(), count-1); } },
    { 'w', repeated(make_select<SelectMode::Replace>(select_to_next_word<Word>)) },
    { 'e', repeated(make_select<SelectMode::Replace>(select_to_next_word_end<Word>)) },
    { 'b', repeated(make_select<SelectMode::Replace>(select_to_previous_word<Word>)) },
    { 'W', repeated(make_select<SelectMode::Extend>(select_to_next_word<Word>)) },
    { 'E', repeated(make_select<SelectMode::Extend>(select_to_next_word_end<Word>)) },
    { 'B', repeated(make_select<SelectMode::Extend>(select_to_previous_word<Word>)) },

    { alt('w'), repeated(make_select<SelectMode::Replace>(select_to_next_word<WORD>)) },
    { alt('e'), repeated(make_select<SelectMode::Replace>(select_to_next_word_end<WORD>)) },
    { alt('b'), repeated(make_select<SelectMode::Replace>(select_to_previous_word<WORD>)) },
    { alt('W'), repeated(make_select<SelectMode::Extend>(select_to_next_word<WORD>)) },
    { alt('E'), repeated(make_select<SelectMode::Extend>(select_to_next_word_end<WORD>)) },
    { alt('B'), repeated(make_select<SelectMode::Extend>(select_to_previous_word<WORD>)) },

    { alt('l'), repeated(make_select<SelectMode::Replace>(select_to_eol)) },
    { alt('L'), repeated(make_select<SelectMode::Extend>(select_to_eol)) },
    { alt('h'), repeated(make_select<SelectMode::Replace>(select_to_eol_reverse)) },
    { alt('H'), repeated(make_select<SelectMode::Extend>(select_to_eol_reverse)) },

    { 'x', repeated(make_select<SelectMode::Replace>(select_line)) },
    { 'X', repeated(make_select<SelectMode::Extend>(select_line)) },
    { alt('x'), make_select<SelectMode::Replace>(select_whole_lines) },
    { alt('X'), make_select<SelectMode::Replace>(trim_partial_lines) },

    { 'm', make_select<SelectMode::Replace>(select_matching) },
    { 'M', make_select<SelectMode::Extend>(select_matching) },

    { '/', search<SelectMode::Replace, Forward> },
    { '?', search<SelectMode::Extend, Forward> },
    { alt('/'), search<SelectMode::Replace, Backward> },
    { alt('?'), search<SelectMode::Extend, Backward> },
    { 'n', search_next<SelectMode::Replace, Forward> },
    { alt('n'), search_next<SelectMode::ReplaceMain, Forward> },
    { 'N', search_next<SelectMode::Append, Forward> },
    { '*', use_selection_as_search_pattern<true> },
    { alt('*'), use_selection_as_search_pattern<false> },

    { 'u', undo },
    { 'U', redo },

    { alt('i'), select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner> },
    { alt('a'), select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd> },
    { ']', select_object<ObjectFlags::ToEnd> },
    { '[', select_object<ObjectFlags::ToBegin> },
    { '{', select_object<ObjectFlags::ToBegin, SelectMode::Extend> },
    { '}', select_object<ObjectFlags::ToEnd, SelectMode::Extend> },

    { alt('j'), join },
    { alt('J'), join_select_spaces },

    { alt('k'), keep<true> },
    { alt('K'), keep<false> },
    { '$', keep_pipe },

    { '<', deindent<true> },
    { '>', indent<false> },
    { alt('>'), indent<true> },
    { alt('<'), deindent<false> },

    { ctrl('i'), jump<Forward> },
    { ctrl('o'), jump<Backward> },
    { ctrl('s'), save_selections },

    { alt('r'), rotate_selections },
    { alt('R'), rotate_selections_content },

    { 'q', replay_macro },
    { 'Q', start_or_end_macro_recording },

    { '`', for_each_char<to_lower> },
    { '~', for_each_char<to_upper> },
    { alt('`'),  for_each_char<swap_case> },

    { '&', align },
    { alt('&'), copy_indent },

    { '@', tabs_to_spaces },
    { alt('@'), spaces_to_tabs },

    { Key::Left,  move<CharCount, Backward> },
    { Key::Down,  move<LineCount, Forward> },
    { Key::Up,    move<LineCount, Backward> },
    { Key::Right, move<CharCount, Forward> },

    { Key::PageUp,   scroll<Key::PageUp> },
    { Key::PageDown, scroll<Key::PageDown> },
};

}
