#include "normal.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "changes.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "commands.hh"
#include "context.hh"
#include "diff.hh"
#include "face_registry.hh"
#include "file.hh"
#include "flags.hh"
#include "option_manager.hh"
#include "option_types.hh"
#include "ranges.hh"
#include "regex.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "user_interface.hh"
#include "unit_tests.hh"
#include "window.hh"
#include "word_db.hh"

namespace Kakoune
{

using namespace std::placeholders;

enum class SelectMode
{
    Replace,
    Extend,
    Append,
};

void merge_selections(Selection& sel, const Selection& new_sel)
{
    const bool forward = sel.cursor() >= sel.anchor();
    const bool new_forward = new_sel.cursor() > new_sel.anchor();
    if (forward and new_forward)
        sel.anchor() = std::min(sel.anchor(), new_sel.anchor());
    const bool backward = sel.cursor() <= sel.anchor();
    const bool new_backward = new_sel.cursor() < new_sel.anchor();
    if (backward and new_backward)
        sel.anchor() = std::max(sel.anchor(), new_sel.anchor());

    sel.cursor() = new_sel.cursor();
}

UnitTest test_merge_selection{[] {
    auto merge = [](Selection sel, const Selection& new_sel) {
        merge_selections(sel, new_sel);
        return sel;
    };
    kak_assert(merge({{0, 1}, {0, 2} }, {{0, 3}, {0, 4}}) == Selection{{0, 1}, {0, 4}});
    kak_assert(merge({{0, 1}, {0, 2} }, {{0, 1}, {0, 2}}) == Selection{{0, 1}, {0, 2}});
    kak_assert(merge({{0, 1}, {0, 2} }, {{0, 0}, {0, 0}}) == Selection{{0, 1}, {0, 0}});
    kak_assert(merge({{0, 1}, {0, 2} }, {{0, 0}, {0, 3}}) == Selection{{0, 0}, {0, 3}});
    kak_assert(merge({{0, 1}, {0, 3} }, {{0, 4}, {0, 2}}) == Selection{{0, 1}, {0, 2}});
    kak_assert(merge({{0, 1}, {0, 2} }, {{0, 1}, {0, 1}}) == Selection{{0, 1}, {0, 1}});
}};

template<SelectMode mode, typename T>
void select(Context& context, T func)
{
    auto& selections = context.selections();
    if (mode == SelectMode::Append)
    {
        auto& sel = selections.main();
        if (auto res = func(context, sel))
        {
            if (res->captures().empty())
                res->captures() = sel.captures();
            selections.push_back(std::move(*res));
            selections.set_main_index(selections.size() - 1);
        }
    }
    else
    {
        Vector<int> to_remove;
        for (int i = 0; i < (int)selections.size(); ++i)
        {
            auto& sel = selections[i];
            auto res = func(context, sel);
            if (not res)
            {
                to_remove.push_back(i);
                continue;
            }

            if (mode == SelectMode::Extend)
                merge_selections(sel, *res);
            else
            {
                sel.anchor() = res->anchor();
                sel.cursor() = res->cursor();
            }
            if (not res->captures().empty())
                sel.captures() = std::move(res->captures());
        }

        if (to_remove.size() == selections.size())
            throw runtime_error{"no selections remaining"};
        for (auto& i : to_remove | reverse())
            selections.remove(i);
    }

    selections.sort_and_merge_overlapping();
    selections.check_invariant();
}

template<SelectMode mode, Optional<Selection> (*func)(const Context&, const Selection&)>
void select(Context& context, NormalParams)
{
    select<mode>(context, func);
}

template<SelectMode mode, typename Func>
void select_and_set_last(Context& context, Func&& func)
{
    context.set_last_select(
        [func](Context& context){ select<mode>(context, func); });
    return select<mode>(context, func);
}

template<SelectMode mode = SelectMode::Replace>
void select_coord(Buffer& buffer, BufferCoord coord, SelectionList& selections)
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
void enter_insert_mode(Context& context, NormalParams params)
{
    context.input_handler().insert(mode, params.count);
}

void repeat_last_insert(Context& context, NormalParams)
{
    context.input_handler().repeat_last_insert();
}

void repeat_last_select(Context& context, NormalParams)
{
    context.repeat_last_select();
}

String build_autoinfo_for_mapping(Context& context, KeymapMode mode,
                                  ConstArrayView<KeyInfo> built_ins)
{
    auto& keymaps = context.keymaps();

    Vector<std::pair<String, StringView>> descs;
    for (auto& built_in : built_ins)
    {
        String keys = join(built_in.keys |
                           filter([&](Key k){ return not keymaps.is_mapped(k, mode); }) |
                           transform(key_to_str),
                           ',', false);
        if (not keys.empty())
            descs.emplace_back(std::move(keys), built_in.docstring);
    }

    for (auto& key : keymaps.get_mapped_keys(mode))
        descs.emplace_back(key_to_str(key),
                           keymaps.get_mapping(key, mode).docstring);

    auto max_len = 0_col;
    for (auto& desc : descs)
    {
        auto len = desc.first.column_length();
        if (len > max_len)
            max_len = len;
    }

    String res;
    for (auto& desc : descs)
        res += format("{}:{}{}\n",
                      desc.first,
                      String{' ', max_len - desc.first.column_length() + 1},
                      desc.second);
    return res;
}

template<SelectMode mode>
void goto_commands(Context& context, NormalParams params)
{
    if (params.count != 0)
    {
        context.push_jump();
        select_coord<mode>(context.buffer(), LineCount{params.count - 1}, context.selections());
        if (context.has_window())
            context.window().center_line(LineCount{params.count-1});
    }
    else
    {
        on_next_key_with_autoinfo(context, KeymapMode::Goto,
                                 [](Key key, Context& context) {
            auto cp = key.codepoint();
            if (not cp or key == Key::Escape)
                return;
            auto& buffer = context.buffer();
            switch (to_lower(*cp))
            {
            case 'g':
            case 'k':
                context.push_jump();
                select_coord<mode>(buffer, BufferCoord{0,0}, context.selections());
                break;
            case 'l':
                select<mode, select_to_line_end<true>>(context, {});
                break;
            case 'h':
                select<mode, select_to_line_begin<true>>(context, {});
                break;
            case 'i':
                select<mode, select_to_first_non_blank>(context, {});
                break;
            case 'j':
                context.push_jump();
                select_coord<mode>(buffer, buffer.line_count() - 1, context.selections());
                break;
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
                Buffer* target = nullptr;
                if (not context.has_client() or
                    not (target = context.client().last_buffer()))
                {
                    throw runtime_error("no last buffer");
                    break;
                }
                context.push_jump();
                context.change_buffer(*target);
                break;
            }
            case 'f':
            {
                auto filename = content(buffer, context.selections().main());
                static constexpr char forbidden[] = { '\'', '\\', '\0' };
                if (any_of(filename, [](char c){ return contains(forbidden, c); }))
                    return;

                auto paths = context.options()["path"].get<Vector<String, MemoryDomain::Options>>();
                const StringView buffer_dir = split_path(buffer.name()).first;
                String path = find_file(filename, buffer_dir, paths);
                if (path.empty())
                    throw runtime_error(format("unable to find file '{}'", filename));

                Buffer* buffer = BufferManager::instance().get_buffer_ifp(path);
                if (not buffer)
                {
                    buffer = open_file_buffer(path, context.hooks_disabled() ?
                                                      Buffer::Flags::NoHooks
                                                    : Buffer::Flags::None);
                    buffer->flags() &= ~Buffer::Flags::NoHooks;
                }

                if (buffer != &context.buffer())
                {
                    context.push_jump();
                    context.change_buffer(*buffer);
                }
                break;
            }
            case '.':
            {
                context.push_jump();
                auto pos = buffer.last_modification_coord();
                if (not pos)
                    throw runtime_error("no last modification position");
                if (*pos >= buffer.back_coord())
                    pos = buffer.back_coord();
                select_coord<mode>(buffer, *pos, context.selections());
                break;
            }
            }
        }, (mode == SelectMode::Extend ? "goto (extend to)" : "goto"),
        build_autoinfo_for_mapping(context, KeymapMode::Goto,
            {{{'g','k'},"buffer top"},
             {{'l'},    "line end"},
             {{'h'},    "line begin"},
             {{'i'},    "line non blank start"},
             {{'j'},    "buffer bottom"},
             {{'e'},    "buffer end"},
             {{'t'},    "window top"},
             {{'b'},    "window bottom"},
             {{'c'},    "window center"},
             {{'a'},    "last buffer"},
             {{'f'},    "file"},
             {{'.'},    "last buffer change"}}));
    }
}

template<bool lock>
void view_commands(Context& context, NormalParams params)
{
    const int count = params.count;
    on_next_key_with_autoinfo(context, KeymapMode::View,
                             [count](Key key, Context& context) {
        if (key == Key::Escape)
            return;

        if (lock)
            view_commands<true>(context, { count, 0 });

        auto cp = key.codepoint();
        if (not cp or not context.has_window())
            return;

        const BufferCoord cursor = context.selections().main().cursor();
        Window& window = context.window();
        switch (to_lower(*cp))
        {
        case 'v':
        case 'c':
            window.center_line(cursor.line);
            break;
        case 'm':
            window.center_column(
                context.buffer()[cursor.line].column_count_to(cursor.column));
            break;
        case 't':
            window.display_line_at(cursor.line, 0);
            break;
        case 'b':
            window.display_line_at(cursor.line, window.dimensions().line-1);
            break;
        case 'h':
            window.scroll(-std::max<ColumnCount>(1, count));
            break;
        case 'j':
            window.scroll( std::max<LineCount>(1, count));
            break;
        case 'k':
            window.scroll(-std::max<LineCount>(1, count));
            break;
        case 'l':
            window.scroll( std::max<ColumnCount>(1, count));
            break;
        }
    }, lock ? "view (lock)" : "view",
    build_autoinfo_for_mapping(context, KeymapMode::View,
        {{{'v','c'}, "center cursor (vertically)"},
         {{'m'},     "center cursor (horizontally)"},
         {{'t'},     "cursor on top"},
         {{'b'},     "cursor on bottom"},
         {{'h'},     "scroll left"},
         {{'j'},     "scroll down"},
         {{'k'},     "scroll up"},
         {{'l'},     "scroll right"}}));
}

void replace_with_char(Context& context, NormalParams)
{
    on_next_key_with_autoinfo(context, KeymapMode::None,
                             [](Key key, Context& context) {
        auto cp = key.codepoint();
        if (not cp or key == Key::Escape)
            return;
        ScopedEdition edition(context);
        Buffer& buffer = context.buffer();
        SelectionList& selections = context.selections();
        Vector<String> strings;
        for (auto& sel : selections)
        {
            CharCount count = char_length(buffer, sel);
            strings.emplace_back(*cp, count);
        }
        selections.insert(strings, InsertMode::Replace);
    }, "replace with char", "enter char to replace with\n");
}

Codepoint swap_case(Codepoint cp)
{
    Codepoint res = to_lower(cp);
    return res == cp ? to_upper(cp) : res;
}

template<Codepoint (*func)(Codepoint)>
void for_each_codepoint(Context& context, NormalParams)
{
    using Utf8It = utf8::iterator<BufferIterator>;

    ScopedEdition edition(context);
    Buffer& buffer = context.buffer();
    SelectionList& selections = context.selections();
    Vector<String> strings;
    for (auto& sel : selections)
    {
        String str;
        for (auto begin = Utf8It{buffer.iterator_at(sel.min()), buffer},
                  end = Utf8It{buffer.iterator_at(sel.max()), buffer}+1;
             begin != end; ++begin)
            utf8::dump(std::back_inserter(str), func(*begin));

        strings.push_back(std::move(str));
    }
    selections.insert(strings, InsertMode::Replace);
}

void command(Context& context, NormalParams params)
{
    if (not CommandManager::has_instance())
        throw runtime_error{"commands are not supported"};

    CommandManager::instance().clear_last_complete_command();

    context.input_handler().prompt(
        ":", {}, context.main_sel_register_value(':').str(),
        context.faces()["Prompt"], PromptFlags::DropHistoryEntriesWithBlankPrefix,
        [](const Context& context, CompletionFlags flags,
           StringView cmd_line, ByteCount pos) {
               return CommandManager::instance().complete(context, flags, cmd_line, pos);
        },
        [params](StringView cmdline, PromptEvent event, Context& context) {
            if (context.has_client())
            {
                context.client().info_hide();
                if (event == PromptEvent::Change)
                {
                    auto info = CommandManager::instance().command_info(context, cmdline);
                    context.input_handler().set_prompt_face(context.faces()[info ? "Prompt" : "Error"]);

                    auto autoinfo = context.options()["autoinfo"].get<AutoInfo>();
                    if (autoinfo & AutoInfo::Command)
                    {
                        if (cmdline.length() == 1 and is_horizontal_blank(cmdline[0_byte]))
                            context.client().info_show("prompt",
                                                       "commands preceded by a blank wont be saved to history",
                                                       {}, InfoStyle::Prompt);
                        else if (info and not info->info.empty())
                            context.client().info_show(info->name, info->info, {}, InfoStyle::Prompt);
                    }
                }
            }
            if (event == PromptEvent::Validate)
            {
                if (cmdline.empty())
                    cmdline = context.main_sel_register_value(':');
                else if (not is_blank(cmdline[0]))
                    RegisterManager::instance()[':'].set(context, cmdline.str());

                EnvVarMap env_vars = {
                    { "count", to_string(params.count) },
                    { "register", String{&params.reg, 1} }
                };
                CommandManager::instance().execute(
                    cmdline, context, { {}, env_vars });
            }
        });
}

void apply_diff(Buffer& buffer, BufferCoord pos, StringView before, StringView after)
{
    // The diff algorithm is O(ND) with N the sum of string len, and D the diff count
    // do not use it if our data is too big
    constexpr ByteCount size_limit = 100 * 1024;
    if (before.length() + after.length() > size_limit)
    {
        buffer.erase(pos, buffer.advance(pos, before.length()));
        buffer.insert(pos, after);
        return;
    }

    auto diffs = find_diff(before.begin(), (int)before.length(), after.begin(), (int)after.length());

    for (auto& diff : diffs)
    {
        switch (diff.mode)
        {
        case Diff::Keep:
            pos = buffer.advance(pos, diff.len);
            break;
        case Diff::Add:
            buffer.insert(pos, after.substr(ByteCount{diff.posB}, ByteCount{diff.len}));
            pos = buffer.advance(pos, diff.len);
            break;
        case Diff::Remove:
            buffer.erase(pos, buffer.advance(pos, diff.len));
            break;
        }
    }
}

template<bool replace>
void pipe(Context& context, NormalParams)
{
    const char* prompt = replace ? "pipe:" : "pipe-to:";
    context.input_handler().prompt(
        prompt, {}, context.main_sel_register_value("|").str(), context.faces()["Prompt"],
        PromptFlags::DropHistoryEntriesWithBlankPrefix,
        shell_complete,
        [](StringView cmdline, PromptEvent event, Context& context)
        {
            if (event != PromptEvent::Validate)
                return;

            if (cmdline.empty())
                cmdline = context.main_sel_register_value("|");
            else
                RegisterManager::instance()['|'].set(context, cmdline.str());

            if (cmdline.empty())
                return;

            Buffer& buffer = context.buffer();
            SelectionList selections = context.selections();
            auto restore_sels = on_scope_end([&, old_main = selections.main_index()] {
                selections.set_main_index(old_main);
                context.selections() = std::move(selections);
            });
            if (replace)
            {
                ScopedEdition edition(context);
                ForwardChangesTracker changes_tracker;
                size_t timestamp = buffer.timestamp();
                for (int i = 0; i < selections.size(); ++i)
                {
                    selections.set_main_index(i);
                    auto& sel = selections.main();
                    const auto beg = changes_tracker.get_new_coord_tolerant(sel.min());
                    const auto end = changes_tracker.get_new_coord_tolerant(sel.max());

                    String in = buffer.string(beg, buffer.char_next(end));
                    const bool insert_eol = in.back() != '\n';
                    if (insert_eol)
                        in += '\n';

                    // Needed in case we read selections inside the cmdline
                    context.selections_write_only() = selections;

                    String out = ShellManager::instance().eval(
                        cmdline, context, in,
                        ShellManager::Flags::WaitForStdout).first;

                    if (insert_eol)
                    {
                        in.resize(in.length()-1, 0);
                        if (not out.empty() and out.back() == '\n')
                            out.resize(out.length()-1, 0);
                    }
                    apply_diff(buffer, beg, in, out);

                    changes_tracker.update(buffer, timestamp);
                }
            }
            else
            {
                for (int i = 0; i < selections.size(); ++i)
                {
                    selections.set_main_index(i);
                    ShellManager::instance().eval(cmdline, context,
                                                  content(buffer, selections.main()),
                                                  ShellManager::Flags::None);
                }
            }
        });
}

template<InsertMode mode>
void insert_output(Context& context, NormalParams)
{
    const char* prompt = mode == InsertMode::Insert ? "insert-output:" : "append-output:";
    context.input_handler().prompt(
        prompt, {}, context.main_sel_register_value("|").str(), context.faces()["Prompt"],
        PromptFlags::DropHistoryEntriesWithBlankPrefix,
        shell_complete,
        [](StringView cmdline, PromptEvent event, Context& context)
        {
            if (event != PromptEvent::Validate)
                return;

            if (cmdline.empty())
                cmdline = context.main_sel_register_value("|");
            else
                RegisterManager::instance()['|'].set(context, cmdline.str());

            if (cmdline.empty())
                return;

            auto str = ShellManager::instance().eval(
                cmdline, context, {}, ShellManager::Flags::WaitForStdout).first;
            ScopedEdition edition(context);
            context.selections().insert(str, mode);
        });
}

void yank(Context& context, NormalParams params)
{
    const char reg = params.reg ? params.reg : '"';
    RegisterManager::instance()[reg].set(context, context.selections_content());
    context.print_status({ format("yanked {} selections to register {}",
                                  context.selections().size(), reg),
                           context.faces()["Information"] });
}

template<bool yank>
void erase_selections(Context& context, NormalParams params)
{
    if (yank)
    {
        const char reg = params.reg ? params.reg : '"';
        RegisterManager::instance()[reg].set(context, context.selections_content());
    }
    ScopedEdition edition(context);
    context.selections().erase();
}

template<bool yank>
void change(Context& context, NormalParams params)
{
    if (yank)
    {
        const char reg = params.reg ? params.reg : '"';
        RegisterManager::instance()[reg].set(context, context.selections_content());
    }
    enter_insert_mode<InsertMode::Replace>(context, params);
}

InsertMode adapt_for_linewise(InsertMode mode)
{
    switch (mode)
    {
        case InsertMode::Append: return InsertMode::InsertAtNextLineBegin;
        case InsertMode::Insert: return InsertMode::InsertAtLineBegin;
        case InsertMode::Replace: return InsertMode::Replace;
        default: return InsertMode::Insert;
    }
}

template<InsertMode mode>
void paste(Context& context, NormalParams params)
{
    const char reg = params.reg ? params.reg : '"';
    auto strings = RegisterManager::instance()[reg].get(context);
    const bool linewise = any_of(strings, [](StringView str) {
        return not str.empty() and str.back() == '\n';
    });
    const auto effective_mode = linewise ? adapt_for_linewise(mode) : mode;

    ScopedEdition edition(context);
    context.selections().insert(strings, effective_mode);
}

template<InsertMode mode>
void paste_all(Context& context, NormalParams params)
{
    const char reg = params.reg ? params.reg : '"';
    auto strings = RegisterManager::instance()[reg].get(context);
    InsertMode effective_mode = mode;
    String all;
    Vector<ByteCount> offsets;
    for (auto& str : strings)
    {
        if (str.empty())
            continue;

        if (str.back() == '\n')
            effective_mode = adapt_for_linewise(mode);
        all += str;
        offsets.push_back(all.length());
    }

    Vector<BufferCoord> insert_pos;
    auto& selections = context.selections();
    {
        ScopedEdition edition(context);
        selections.insert(all, effective_mode, &insert_pos);
    }

    const Buffer& buffer = context.buffer();
    Vector<Selection> result;
    for (auto& ins_pos : insert_pos)
    {
        ByteCount pos = 0;
        for (auto offset : offsets)
        {
            result.emplace_back(buffer.advance(ins_pos, pos),
                                buffer.advance(ins_pos, offset-1));
            pos = offset;
        }
    }
    if (not result.empty())
        selections = std::move(result);
}

constexpr RegexCompileFlags direction_flags(RegexMode mode)
{
    return (mode & RegexMode::Forward) ?
        RegexCompileFlags::None : RegexCompileFlags::Backward | RegexCompileFlags::NoForward;
}

template<RegexMode mode = RegexMode::Forward, typename T>
void regex_prompt(Context& context, String prompt, String default_regex, T func)
{
    static_assert(is_direction(mode));
    DisplayCoord position = context.has_window() ? context.window().position() : DisplayCoord{};
    SelectionList selections = context.selections();
    context.input_handler().prompt(
        std::move(prompt), {}, default_regex, context.faces()["Prompt"],
        PromptFlags::Search,
        [](const Context& context, CompletionFlags, StringView regex, ByteCount pos) -> Completions {
            auto current_word = [](StringView s) {
                auto it = s.end();
                while (it != s.begin() and is_word(*(it-1)))
                    --it;
                StringView res{it, s.end()};
                if (it == s.begin() or res.empty())
                    return res;

                int backslashes = 0;
                for (auto bs = it; bs != s.begin() && *(bs-1) == '\\'; --bs)
                    ++backslashes;
                return (backslashes % 2 == 1) ? res.substr(1_byte) : res;
            };

            const auto word = current_word(regex.substr(0_byte, pos));
            auto matches = get_word_db(context.buffer()).find_matching(word);
            constexpr size_t max_count = 100;
            CandidateList candidates;
            candidates.reserve(std::min(matches.size(), max_count));
            for_n_best(matches, max_count, [](auto& lhs, auto& rhs) { return rhs < lhs; },
                       [&](auto&& m) { candidates.push_back(m.candidate().str()); return true; });
            return {(int)(word.begin() - regex.begin()), pos,  std::move(candidates) };
        },
        [=](StringView str, PromptEvent event, Context& context) mutable {
            try
            {
                if (event != PromptEvent::Change and context.has_client())
                    context.client().info_hide();

                const bool incsearch = context.options()["incsearch"].get<bool>();
                if (incsearch)
                {
                    selections.update();
                    context.selections_write_only() = selections;
                    if (context.has_window())
                        context.window().set_position(position);

                    context.input_handler().set_prompt_face(context.faces()["Prompt"]);
                }

                if (not incsearch and event == PromptEvent::Change)
                    return;

                if (event == PromptEvent::Validate)
                    context.push_jump();

                if (not str.empty() or event == PromptEvent::Validate)
                    func(Regex{str.empty() ? default_regex : str, direction_flags(mode)}, event, context);
            }
            catch (regex_error& err)
            {
                if (event == PromptEvent::Validate)
                    throw;
                else
                    context.input_handler().set_prompt_face(context.faces()["Error"]);
            }
            catch (runtime_error&)
            {
                context.selections_write_only() = selections;
                // only validation should propagate errors,
                // incremental search should not.
                if (event == PromptEvent::Validate)
                    throw;
            }
        });
}

template<RegexMode mode>
void select_next_matches(Context& context, const Regex& regex, int count)
{
     auto& selections = context.selections();
     do {
         bool wrapped = false;
         for (auto& sel : selections)
             sel = keep_direction(find_next_match<mode>(context, sel, regex, wrapped), sel);
         selections.sort_and_merge_overlapping();
     } while (--count > 0);
}

template<RegexMode mode>
void extend_to_next_matches(Context& context, const Regex& regex, int count)
{
     Vector<Selection> new_sels;
     auto& selections = context.selections();
     do {
         bool wrapped = false;
         size_t main_index = selections.main_index();
         for (auto& sel : selections)
         {
             auto new_sel = find_next_match<mode>(context, sel, regex, wrapped);
             if (not wrapped)
             {
                 new_sels.push_back(sel);
                 merge_selections(new_sels.back(), new_sel);
             }
             else if (new_sels.size() <= main_index)
                 --main_index;
         }
         if (new_sels.empty())
             throw runtime_error{"All selections wrapped"};

         selections.set(std::move(new_sels), main_index);
         new_sels.clear();
     } while (--count > 0);
}

template<SelectMode mode, RegexMode regex_mode>
void search(Context& context, NormalParams params)
{
    static_assert(is_direction(regex_mode));
    constexpr StringView prompt = mode == SelectMode::Extend ?
        (regex_mode & RegexMode::Forward ? "search (extend):" : "reverse search (extend):")
      : (regex_mode & RegexMode::Forward ? "search:"          : "reverse search:");

    const char reg = to_lower(params.reg ? params.reg : '/');
    const int count = params.count;

    auto reg_content = RegisterManager::instance()[reg].get(context);
    Vector<String> saved_reg{reg_content.begin(), reg_content.end()};
    const int main_index = std::min(context.selections().main_index(), saved_reg.size()-1);

    regex_prompt<regex_mode>(context, prompt.str(), saved_reg[main_index],
                 [reg, count, saved_reg]
                 (const Regex& regex, PromptEvent event, Context& context) {
                     if (event == PromptEvent::Abort)
                     {
                         RegisterManager::instance()[reg].set(context, saved_reg);
                         return;
                     }
                     RegisterManager::instance()[reg].set(context, regex.str());

                     if (regex.empty() or regex.str().empty())
                         return;

                     if (mode == SelectMode::Extend)
                         extend_to_next_matches<regex_mode>(context, regex, count);
                     else
                         select_next_matches<regex_mode>(context, regex, count);
                 });
}

template<SelectMode mode, RegexMode regex_mode>
void search_next(Context& context, NormalParams params)
{
    const char reg = to_lower(params.reg ? params.reg : '/');
    StringView str = context.main_sel_register_value(reg);
    if (not str.empty())
    {
        Regex regex{str, direction_flags(regex_mode)};
        auto& selections = context.selections();
        bool main_wrapped = false;
        do {
            bool wrapped = false;
            if (mode == SelectMode::Replace)
            {
                auto& sel = selections.main();
                sel = keep_direction(find_next_match<regex_mode>(context, sel, regex, wrapped), sel);
            }
            else if (mode == SelectMode::Append)
            {
                auto sel = keep_direction(
                    find_next_match<regex_mode>(context, selections.main(), regex, wrapped),
                    selections.main());
                selections.push_back(std::move(sel));
                selections.set_main_index(selections.size() - 1);
            }
            selections.sort_and_merge_overlapping();
            main_wrapped = main_wrapped or wrapped;
        } while (--params.count > 0);

        if (main_wrapped)
            context.print_status({"main selection search wrapped around buffer", context.faces()["Information"]});
    }
    else
        throw runtime_error("no search pattern");
}

template<bool smart>
void use_selection_as_search_pattern(Context& context, NormalParams params)
{
    Vector<String> patterns;
    auto& sels = context.selections();
    const auto& buffer = context.buffer();
    for (auto& sel : sels)
    {
        const auto beg = sel.min(), end = buffer.char_next(sel.max());
        patterns.push_back(format("{}{}{}",
                                  smart and is_bow(buffer, beg) ? "\\b" : "",
                                  escape(buffer.string(beg, end), "^$\\.*+?()[]{}|", '\\'),
                                  smart and is_eow(buffer, end) ? "\\b" : ""));
    }

    const char reg = to_lower(params.reg ? params.reg : '/');

    context.print_status({
        format("register '{}' set to '{}'", reg, fix_atom_text(patterns[sels.main_index()])),
        context.faces()["Information"] });

    RegisterManager::instance()[reg].set(context, patterns);

    // Hack, as Window do not take register state into account
    if (context.has_window())
        context.window().force_redraw();
}

void select_regex(Context& context, NormalParams params)
{
    const char reg = to_lower(params.reg ? params.reg : '/');
    const int capture = params.count;
    auto prompt = capture ? format("select (capture {}):", capture) :  "select:"_str;

    auto reg_content = RegisterManager::instance()[reg].get(context);
    Vector<String> saved_reg{reg_content.begin(), reg_content.end()};
    const int main_index = std::min(context.selections().main_index(), saved_reg.size()-1);

    regex_prompt(context, std::move(prompt), saved_reg[main_index],
                 [reg, capture, saved_reg](Regex ex, PromptEvent event, Context& context) {
         if (event == PromptEvent::Abort)
         {
             RegisterManager::instance()[reg].set(context, saved_reg);
             return;
         }

        RegisterManager::instance()[reg].set(context, ex.str());

        if (not ex.empty() and not ex.str().empty())
            select_all_matches(context.selections(), ex, capture);
    });
}

void split_regex(Context& context, NormalParams params)
{
    const char reg = to_lower(params.reg ? params.reg : '/');
    const int capture = params.count;
    auto prompt = capture ? format("split (on capture {}):", (int)capture) :  "split:"_str;

    auto reg_content = RegisterManager::instance()[reg].get(context);
    Vector<String> saved_reg{reg_content.begin(), reg_content.end()};
    const int main_index = std::min(context.selections().main_index(), saved_reg.size()-1);

    regex_prompt(context, std::move(prompt), saved_reg[main_index],
                 [reg, capture, saved_reg](Regex ex, PromptEvent event, Context& context) {
         if (event == PromptEvent::Abort)
         {
             RegisterManager::instance()[reg].set(context, saved_reg);
             return;
         }

        RegisterManager::instance()[reg].set(context, ex.str());

        if (not ex.empty() and not ex.str().empty())
            split_selections(context.selections(), ex, capture);
    });
}

void split_lines(Context& context, NormalParams params)
{
    const LineCount count{params.count == 0 ? 1 : params.count};
    auto& selections = context.selections();
    auto& buffer = context.buffer();
    Vector<Selection> res;
    for (auto& sel : selections)
    {
        if (sel.anchor().line == sel.cursor().line)
        {
             res.push_back(std::move(sel));
             continue;
        }
        auto min = sel.min();
        auto max = sel.max();
        for (auto line = min.line; line <= max.line; line += count)
        {
            auto last_line = std::min(line + count - 1, buffer.line_count() - 1);
            res.push_back(keep_direction({
                    std::max<BufferCoord>(min, line),
                    std::min<BufferCoord>(max, {last_line, buffer[last_line].length() - 1})
                }, sel));
        }
    }
    selections = std::move(res);
}

void select_boundaries(Context& context, NormalParams)
{
    auto& selections = context.selections();
    Vector<Selection> res;
    for (auto& sel : selections)
    {
        res.push_back(sel.min());
        if (sel.min() != sel.max())
            res.push_back(sel.max());
    }
    selections = std::move(res);
}

void join_lines_select_spaces(Context& context, NormalParams)
{
    auto& buffer = context.buffer();
    Vector<Selection> selections;
    for (auto& sel : context.selections())
    {
        const LineCount min_line = sel.min().line;
        const LineCount max_line = sel.max().line;
        auto end_line = std::min(buffer.line_count()-1,
                                 max_line + (min_line == max_line ? 1 : 0));
        for (LineCount line = min_line; line < end_line; ++line)
        {
            auto begin = buffer.iterator_at({line, buffer[line].length()-1});
            auto end = std::find_if_not(begin+1, buffer.end(), is_horizontal_blank);
            selections.emplace_back(begin.coord(), (end-1).coord());
        }
    }
    if (selections.empty())
        return;
    context.selections_write_only() = std::move(selections);
    ScopedEdition edition(context);
    context.selections().insert(" "_str, InsertMode::Replace);
}

void join_lines(Context& context, NormalParams params)
{
    SelectionList sels{context.selections()};
    auto restore_sels = on_scope_end([&]{
        sels.update();
        context.selections_write_only() = std::move(sels);
    });

    join_lines_select_spaces(context, params);
}

template<bool matching>
void keep(Context& context, NormalParams params)
{
    constexpr StringView prompt = matching ? "keep matching:" : "keep not matching:";

    const char reg = to_lower(params.reg ? params.reg : '/');
    auto saved_reg = RegisterManager::instance()[reg].get(context) | gather<Vector<String>>();
    const int main_index = std::min(context.selections().main_index(), saved_reg.size()-1);

    regex_prompt(context, prompt.str(), saved_reg[main_index],
                 [saved_reg, reg]
                 (const Regex& regex, PromptEvent event, Context& context) {

        if (event == PromptEvent::Abort)
        {
            RegisterManager::instance()[reg].set(context, saved_reg);
            return;
        }
        if (not context.history_disabled())
            RegisterManager::instance()[reg].set(context, regex.str());

        if (regex.empty() or regex.str().empty())
            return;

        const Buffer& buffer = context.buffer();
        Vector<Selection> keep;
        for (auto& sel : context.selections())
        {
            auto begin = buffer.iterator_at(sel.min());
            auto end = utf8::next(buffer.iterator_at(sel.max()), buffer.end());
            // We do not consider if end is on an eol, as it seems to
            // give more intuitive behaviours in keep use cases.
            const auto flags = match_flags(is_bol(begin.coord()), false,
                                           is_bow(buffer, begin.coord()),
                                           is_eow(buffer, end.coord()));
            if (regex_search(begin, end, begin, end, regex, flags) == matching)
                keep.push_back(sel);
        }
        if (keep.empty())
            throw runtime_error("no selections remaining");
        context.selections_write_only() = std::move(keep);
    });
}

void keep_pipe(Context& context, NormalParams)
{
    context.input_handler().prompt(
        "keep pipe:", {}, {}, context.faces()["Prompt"],
        PromptFlags::DropHistoryEntriesWithBlankPrefix, shell_complete,
        [](StringView cmdline, PromptEvent event, Context& context) {
            if (event != PromptEvent::Validate)
                return;
            const Buffer& buffer = context.buffer();
            auto& shell_manager = ShellManager::instance();
            Vector<Selection> keep;

            auto& selections = context.selections();
            const size_t old_main = selections.main_index();
            size_t new_main = -1;
            for (int i = 0; i < selections.size(); ++i)
            {
                auto& sel = selections[i];
                selections.set_main_index(i);
                if (shell_manager.eval(cmdline, context, content(buffer, sel),
                                       ShellManager::Flags::None).second == 0)
                {
                    keep.push_back(sel);
                    if (i >= old_main and new_main == (size_t)-1)
                        new_main = keep.size() - 1;
                }
            }
            if (keep.empty())
                throw runtime_error("no selections remaining");
            if (new_main == -1)
                new_main = keep.size() - 1;
            context.selections_write_only().set(std::move(keep), new_main);
    });
}
template<bool indent_empty = false>
void indent(Context& context, NormalParams params)
{
    CharCount count = params.count ? params.count : 1;
    CharCount indent_width = context.options()["indentwidth"].get<int>();
    String indent = indent_width == 0 ? String{'\t', count} : String{' ', indent_width * count};

    auto& buffer = context.buffer();
    Vector<Selection> sels;
    LineCount last_line = 0;
    for (auto& sel : context.selections())
    {
        for (auto line = std::max(last_line, sel.min().line); line < sel.max().line+1; ++line)
        {
            if (indent_empty or buffer[line].length() > 1)
                sels.emplace_back(line, line);
        }
        // avoid reindenting the same line if multiple selections are on it
        last_line = sel.max().line+1;
    }
    if (not sels.empty())
    {
        ScopedEdition edition(context);
        SelectionList selections{buffer, std::move(sels)};
        selections.insert(indent, InsertMode::Insert);
    }
}

template<bool deindent_incomplete = true>
void deindent(Context& context, NormalParams params)
{
    ColumnCount count = params.count ? params.count : 1;
    ColumnCount tabstop = context.options()["tabstop"].get<int>();
    ColumnCount indent_width = context.options()["indentwidth"].get<int>();
    if (indent_width == 0)
        indent_width = tabstop;
    indent_width = indent_width * count;

    auto& buffer = context.buffer();
    Vector<Selection> sels;
    LineCount last_line = 0;
    for (auto& sel : context.selections())
    {
        for (auto line = std::max(sel.min().line, last_line);
             line < sel.max().line+1; ++line)
        {
            ColumnCount width = 0;
            auto content = buffer[line];
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
                        sels.emplace_back(line, BufferCoord{line, column-1});
                    break;
                }
                if (width >= indent_width)
                {
                    sels.emplace_back(line, BufferCoord{line, column});
                    break;
                }
            }
        }
        // avoid reindenting the same line if multiple selections are on it
        last_line = sel.max().line + 1;
    }
    if (not sels.empty())
    {
        ScopedEdition edition(context);
        SelectionList selections{context.buffer(), std::move(sels)};
        selections.erase();
    }
}

template<ObjectFlags flags, SelectMode mode = SelectMode::Replace>
void select_object(Context& context, NormalParams params)
{
    auto get_title = [] {
        const auto whole_flags = (ObjectFlags::ToBegin | ObjectFlags::ToEnd);
        const bool whole = (flags & whole_flags) == whole_flags;
        return format("{} {}{}surrounding object{}",
                      mode == SelectMode::Extend ? "extend" : "select",
                      whole ? "" : "to ",
                      flags & ObjectFlags::Inner ? "inner " : "",
                      whole ? "" : (flags & ObjectFlags::ToBegin ? " begin" : " end"));
    };

    const int count = params.count <= 0 ? 0 : params.count - 1;
    on_next_key_with_autoinfo(context, KeymapMode::Object,
                             [count](Key key, Context& context) {
        if (key == Key::Escape)
            return;

        static constexpr struct ObjectType
        {
            Key key;
            Optional<Selection> (*func)(const Context&, const Selection&, int, ObjectFlags);
        } selectors[] = {
            { 'w', select_word<Word> },
            { alt('w'), select_word<WORD> },
            { 's', select_sentence },
            { 'p', select_paragraph },
            { ' ', select_whitespaces },
            { 'i', select_indent },
            { 'n', select_number },
            { 'u', select_argument },
        };
        auto obj_it = find(selectors | transform(&ObjectType::key), key).base();
        if (obj_it != std::end(selectors))
            return select_and_set_last<mode>(
                context, std::bind(obj_it->func, _1, _2, count, flags));

        if (key == 'c')
        {
            const bool info = show_auto_info_ifn(
                "Enter object desc",
                "format: <open regex>,<close regex>\n"
                "        escape commas with '\\'",
                AutoInfo::Command, context);

            context.input_handler().prompt(
                "object desc:", {}, {}, context.faces()["Prompt"],
                PromptFlags::None, complete_nothing,
                [count,info](StringView cmdline, PromptEvent event, Context& context) {
                    if (event != PromptEvent::Change)
                        hide_auto_info_ifn(context, info);
                    if (event != PromptEvent::Validate)
                        return;

                    struct error : runtime_error { error(size_t) : runtime_error{"desc parsing failed, expected <open>,<close>"} {} };

                    auto params = cmdline | split<StringView>(',', '\\') |
                        transform(unescape<',', '\\'>) | static_gather<error, 2>();

                    if (params[0].empty() or params[1].empty())
                        throw error{0};

                    select_and_set_last<mode>(
                        context, std::bind(select_surrounding, _1, _2,
                                           Regex{params[0], RegexCompileFlags::Backward},
                                           Regex{params[1], RegexCompileFlags::Backward},
                                           count, flags));
                });
            return;
        }

        static constexpr struct SurroundingPair
        {
            char opening;
            char closing;
            char name;
        } surrounding_pairs[] = {
            { '(', ')', 'b' },
            { '{', '}', 'B' },
            { '[', ']', 'r' },
            { '<', '>', 'a' },
            { '"', '"', 'Q' },
            { '\'', '\'', 'q' },
            { '`', '`', 'g' },
        };
        auto pair_it = find_if(surrounding_pairs,
                               [key](const SurroundingPair& s) {
                                   return key == s.opening or key == s.closing or
                                          (s.name != 0 and key == s.name);
                               });
        if (pair_it != std::end(surrounding_pairs))
            return select_and_set_last<mode>(
                context, std::bind(select_surrounding, _1, _2,
                                   Regex{format("\\Q{}", pair_it->opening), RegexCompileFlags::Backward},
                                   Regex{format("\\Q{}", pair_it->closing), RegexCompileFlags::Backward},
                                   count, flags));

        if (not key.codepoint())
            return;

        const Codepoint cp = *key.codepoint();
        if (is_punctuation(cp, {}))
        {
            auto re = Regex{"\\Q" + to_string(cp), RegexCompileFlags::Backward};
            return select_and_set_last<mode>(
                context, std::bind(select_surrounding, _1, _2,
                                   re, re, count, flags));
        }
    }, get_title(),
    build_autoinfo_for_mapping(context, KeymapMode::Object,
        {{{'b','(',')'}, "parenthesis block"},
         {{'B','{','}'}, "braces block"},
         {{'r','[',']'}, "brackets block"},
         {{'a','<','>'}, "angle block"},
         {{'"','Q'},     "double quote string"},
         {{'\'','q'},    "single quote string"},
         {{'`','g'},     "grave quote string"},
         {{'w'},         "word"},
         {{alt('w')},    "WORD"},
         {{'s'},         "sentence"},
         {{'p'},         "paragraph"},
         {{' '},         "whitespaces"},
         {{'i'},         "indent"},
         {{'u'},         "argument"},
         {{'n'},         "number"},
         {{'c'},         "custom object desc"}}));
}

enum Direction { Backward = -1, Forward = 1 };

template<Direction direction, bool half = false>
void scroll(Context& context, NormalParams params)
{
    Window& window = context.window();
    const int count = params.count ? params.count : 1;
    const LineCount offset = (window.dimensions().line - 2) / (half ? 2 : 1) * count;

    scroll_window(context, offset * direction);
}

template<Direction direction>
void copy_selections_on_next_lines(Context& context, NormalParams params)
{
    auto& selections = context.selections();
    auto& buffer = context.buffer();
    const ColumnCount tabstop = context.options()["tabstop"].get<int>();
    Vector<Selection> result;
    size_t main_index = 0;
    for (auto& sel : selections)
    {
        const bool is_main = (&sel == &selections.main());
        auto anchor = sel.anchor();
        auto cursor = sel.cursor();
        ColumnCount cursor_col = get_column(buffer, tabstop, cursor);
        ColumnCount anchor_col = get_column(buffer, tabstop, anchor);

        if (is_main)
            main_index = result.size();
        result.push_back(std::move(sel));
        const LineCount height = std::max(anchor.line, cursor.line) - std::min(anchor.line, cursor.line) + 1;
        const size_t max_lines = std::max(params.count, 1);

        for (size_t i = 0, nb_sels = 0; nb_sels < max_lines; ++i)
        {
            LineCount offset = direction * (i + 1) * height;

            const LineCount anchor_line = anchor.line + offset;
            const LineCount cursor_line = cursor.line + offset;

            if (anchor_line < 0 or cursor_line < 0 or
                anchor_line >= buffer.line_count() or cursor_line >= buffer.line_count())
                break;

            const ByteCount anchor_byte = get_byte_to_column(buffer, tabstop, {anchor_line, anchor_col});
            const ByteCount cursor_byte = get_byte_to_column(buffer, tabstop, {cursor_line, cursor_col});

            if (anchor_byte != buffer[anchor_line].length() and
                cursor_byte != buffer[cursor_line].length())
            {
                if (is_main)
                    main_index = result.size();
                result.emplace_back(BufferCoord{anchor_line, anchor_byte},
                                    BufferCoordAndTarget{cursor_line, cursor_byte, cursor.target});

                nb_sels++;
            }
        }
    }
    selections.set(std::move(result), main_index);
    selections.sort_and_merge_overlapping();
}

template<Direction direction>
void rotate_selections(Context& context, NormalParams params)
{
    const int count = params.count ? params.count : 1;
    auto& selections = context.selections();
    const int index = selections.main_index();
    const int num = selections.size();
    selections.set_main_index((direction == Forward) ?
                                (index + count) % num
                              : (index + (num - count % num)) % num);
}

template<Direction direction>
void rotate_selections_content(Context& context, NormalParams params)
{
    size_t group = params.count;
    size_t count = 1;
    auto strings = context.selections_content();
    if (group == 0 or group > (int)strings.size())
        group = (int)strings.size();
    count = count % group;
    auto& selections = context.selections();
    auto main = strings.begin() + selections.main_index();
    for (auto it = strings.begin(); it != strings.end(); )
    {
        auto end = std::min(strings.end(), it + group);
        auto new_beg = (direction == Direction::Forward) ? end - count : it + count;
        std::rotate(it, new_beg, end);

        if (it <= main and main < end)
            main = main < new_beg ? end - (new_beg - main) : it + (main - new_beg);
        it = end;
    }
    selections.insert(strings, InsertMode::Replace);
    selections.set_main_index(main - strings.begin());
}

enum class SelectFlags
{
    None = 0,
    Reverse = 1,
    Inclusive = 2,
    Extend = 4
};

constexpr bool with_bit_ops(Meta::Type<SelectFlags>) { return true; }

template<SelectFlags flags>
void select_to_next_char(Context& context, NormalParams params)
{
    auto get_title = [] {
        return format("{} {} {} char",
                      flags & SelectFlags::Extend ? "extend" : "select",
                      flags & SelectFlags::Inclusive ? "onto" : "to",
                      flags & SelectFlags::Reverse ? "previous" : "next");
    };

    on_next_key_with_autoinfo(context, KeymapMode::None,
                             [params](Key key, Context& context) {
        auto cp = key.codepoint();
        if (not cp or key == Key::Escape)
            return;
        constexpr auto new_flags = flags & SelectFlags::Extend ? SelectMode::Extend
                                                               : SelectMode::Replace;
        select_and_set_last<new_flags>(
            context,
            std::bind(flags & SelectFlags::Reverse ? select_to_reverse
                                                   : select_to,
                      _1, _2, *cp, params.count,
                      flags & SelectFlags::Inclusive));
    }, get_title(), "enter char to select to");
}

void start_or_end_macro_recording(Context& context, NormalParams params)
{
    if (context.input_handler().is_recording())
        context.input_handler().stop_recording();
    else
    {
        const char reg = to_lower(params.reg ? params.reg : '@');
        if (not is_basic_alpha(reg) and reg != '@')
            throw runtime_error("macros can only use the '@' and alphabetic registers");
        context.input_handler().start_recording(reg);
    }
}

void end_macro_recording(Context& context, NormalParams)
{
    if (context.input_handler().is_recording())
        context.input_handler().stop_recording();
}

void replay_macro(Context& context, NormalParams params)
{
    const char reg = to_lower(params.reg ? params.reg : '@');
    if (not is_basic_alpha(reg) and reg != '@')
        throw runtime_error("macros can only use the '@' and alphabetic registers");

    static bool running_macros[27] = {};
    const size_t idx = reg != '@' ? (size_t)(reg - 'a') : 26;
    if (running_macros[idx])
        throw runtime_error("recursive macros call detected");

    ConstArrayView<String> reg_val = RegisterManager::instance()[reg].get(context);
    if (reg_val.empty() or reg_val[0].empty())
        throw runtime_error(format("register '{}' is empty", reg));

    running_macros[idx] = true;
    auto stop = on_scope_end([&]{ running_macros[idx] = false; });

    auto keys = parse_keys(reg_val[0]);
    ScopedEdition edition(context);
    do
    {
        for (auto& key : keys)
            context.input_handler().handle_key(key);
    } while (--params.count > 0);
}

template<Direction direction>
void jump(Context& context, NormalParams params)
{
    const int count = std::max(1, params.count);
    auto jump = (direction == Forward) ?
                 context.jump_list().forward(context, count) :
                 context.jump_list().backward(context, count);

    Buffer* oldbuf = &context.buffer();
    Buffer& buffer = const_cast<Buffer&>(jump.buffer());
    if (&buffer != oldbuf)
        context.change_buffer(buffer);
    context.selections_write_only() = jump;
}

void push_selections(Context& context, NormalParams)
{
    context.push_jump(true);
    context.print_status({ format("saved {} selections", context.selections().size()),
                           context.faces()["Information"] });
}

void align(Context& context, NormalParams)
{
    auto& selections = context.selections();
    auto& buffer = context.buffer();
    const ColumnCount tabstop = context.options()["tabstop"].get<int>();

    Vector<Vector<const Selection*>> columns;
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
        ColumnCount maxcol = 0;
        for (auto& sel : col)
            maxcol = std::max(get_column(buffer, tabstop, sel->cursor()), maxcol);
        for (auto& sel : col)
        {
            auto insert_coord = sel->min();
            ColumnCount lastcol = get_column(buffer, tabstop, sel->cursor());
            ColumnCount inscount = maxcol - lastcol;
            String padstr;
            if (not use_tabs)
                padstr = String{ ' ', inscount };
            else
            {
                ColumnCount inscol = get_column(buffer, tabstop, insert_coord);
                ColumnCount targetcol = inscol + inscount;
                ColumnCount tabcol = inscol - (inscol % tabstop);
                CharCount tabs = (int)((targetcol - tabcol) / tabstop);
                CharCount spaces = (int)(targetcol - (tabs ? (tabcol + (int)tabs * tabstop) : inscol));
                padstr = String{ '\t', tabs } + String{ ' ', spaces };
            }
            buffer.insert(insert_coord, padstr);
        }
        selections.update();
    }
}

void copy_indent(Context& context, NormalParams params)
{
    int selection = params.count;
    auto& buffer = context.buffer();
    auto& selections = context.selections();
    Vector<LineCount> lines;
    for (auto sel : selections)
    {
        for (LineCount l = sel.min().line; l < sel.max().line + 1; ++l)
            lines.push_back(l);
    }
    if (selection > selections.size())
        throw runtime_error("invalid selection index");
    if (selection == 0)
        selection = context.selections().main_index() + 1;

    auto ref_line = selections[selection-1].min().line;
    auto line = buffer[ref_line];
    auto it = line.begin();
    while (it != line.end() and is_horizontal_blank(*it))
        ++it;
    const StringView indent = line.substr(0_byte, (int)(it-line.begin()));

    ScopedEdition edition{context};
    for (auto& l : lines)
    {
        if (l == ref_line)
            continue;

        auto line = buffer[l];
        ByteCount i = 0;
        while (i < line.length() and is_horizontal_blank(line[i]))
            ++i;
        buffer.replace(l, {l, i}, indent);
    }
}

void tabs_to_spaces(Context& context, NormalParams params)
{
    auto& buffer = context.buffer();
    const ColumnCount opt_tabstop = context.options()["tabstop"].get<int>();
    const ColumnCount tabstop = params.count == 0 ? opt_tabstop : params.count;
    Vector<Selection> tabs;
    Vector<String> spaces;
    for (auto& sel : context.selections())
    {
        for (auto it = buffer.iterator_at(sel.min()),
                  end = buffer.iterator_at(sel.max())+1; it != end; ++it)
        {
            if (*it == '\t')
            {
                ColumnCount col = get_column(buffer, opt_tabstop, it.coord());
                ColumnCount end_col = (col / tabstop + 1) * tabstop;
                tabs.emplace_back(it.coord());
                spaces.emplace_back(' ', end_col - col);
            }
        }
    }
    if (not tabs.empty())
        SelectionList{ buffer, std::move(tabs) }.insert(spaces, InsertMode::Replace);
}

void spaces_to_tabs(Context& context, NormalParams params)
{
    auto& buffer = context.buffer();
    const ColumnCount opt_tabstop = context.options()["tabstop"].get<int>();
    const ColumnCount tabstop = params.count == 0 ? opt_tabstop : params.count;
    Vector<Selection> spaces;
    for (auto& sel : context.selections())
    {
        for (auto it = buffer.iterator_at(sel.min()),
                  end = buffer.iterator_at(sel.max())+1; it != end;)
        {
            if (*it == ' ')
            {
                auto spaces_beg = it;
                auto spaces_end = spaces_beg+1;
                ColumnCount col = get_column(buffer, opt_tabstop, spaces_end.coord());
                while (spaces_end != end and
                       *spaces_end == ' ' and (col % tabstop) != 0)
                {
                    ++spaces_end;
                    ++col;
                }
                if ((col % tabstop) == 0)
                    spaces.emplace_back(spaces_beg.coord(), (spaces_end-1).coord());
                else if (spaces_end != end and *spaces_end == '\t')
                    spaces.emplace_back(spaces_beg.coord(), spaces_end.coord());
                it = spaces_end;
            }
            else
                ++it;
        }
    }
    if (not spaces.empty())
        SelectionList{ buffer, std::move(spaces) }.insert("\t"_str, InsertMode::Replace);
}

void trim_selections(Context& context, NormalParams)
{
    auto& buffer = context.buffer();
    auto& selections = context.selections();
    Vector<int> to_remove;

    for (int i = 0; i < (int)selections.size(); ++i)
    {
        auto& sel = selections[i];
        auto beg = buffer.iterator_at(sel.min());
        auto end = buffer.iterator_at(sel.max());
        while (beg != end and is_blank(*beg))
            ++beg;
        while (beg != end and is_blank(*end))
            --end;

        if (beg == end and is_blank(*beg))
            to_remove.push_back(i);
        else
        {
            sel.min() = beg.coord();
            sel.max() = end.coord();
        }
    }

    if (to_remove.size() == selections.size())
        throw runtime_error{"no selections remaining"};
    for (auto& i : to_remove | reverse())
        selections.remove(i);
}

SelectionList read_selections_from_register(char reg, Context& context)
{
    if (not is_basic_alpha(reg) and reg != '^')
        throw runtime_error("selections can only be saved to the '^' and alphabetic registers");

    auto content = RegisterManager::instance()[reg].get(context);

    if (content.size() < 2)
        throw runtime_error(format("register '{}' does not contain a selections desc", reg));

    struct error : runtime_error { error(size_t) : runtime_error{"expected <buffer>@<timestamp>@main_index"} {} };
    const auto desc = content[0] | split<StringView>('@') | static_gather<error, 3>();
    Buffer& buffer = BufferManager::instance().get_buffer(desc[0]);
    const size_t timestamp = str_to_int(desc[1]);
    size_t main = str_to_int(desc[2]);

    if (timestamp > buffer.timestamp())
        throw runtime_error{"register '{}' refers to an invalid timestamp"};

    auto sels = content | skip(1) | transform(selection_from_string) | gather<Vector<Selection>>();
    sort_selections(sels, main);
    merge_overlapping_selections(sels, main);
    if (timestamp < buffer.timestamp())
        update_selections(sels, main, buffer, timestamp);
    else
        clamp_selections(sels, buffer);

    SelectionList res{buffer, std::move(sels)};
    res.set_main_index(main);
    return res;
}

enum class CombineOp
{
    Append,
    Union,
    Intersect,
    SelectLeftmostCursor,
    SelectRightmostCursor,
    SelectLongest,
    SelectShortest,
};

CombineOp key_to_combine_op(Key key)
{
    switch (key.key)
    {
        case 'a': return CombineOp::Append;
        case 'u': return CombineOp::Union;
        case 'i': return CombineOp::Intersect;
        case '<': return CombineOp::SelectLeftmostCursor;
        case '>': return CombineOp::SelectRightmostCursor;
        case '+': return CombineOp::SelectLongest;
        case '-': return CombineOp::SelectShortest;
    }
    throw runtime_error{format("no such combine operator: '{}'", key.key)};
}

void combine_selection(const Buffer& buffer, Selection& sel, const Selection& other, CombineOp op)
{
    switch (op)
    {
        case CombineOp::Union:
            sel.set(std::min(sel.min(), other.min()),
                    std::max(sel.max(), other.max()));
            break;
        case CombineOp::Intersect:
            sel.set(std::max(sel.min(), other.min()),
                    std::min(sel.max(), other.max()));
            break;
        case CombineOp::SelectLeftmostCursor:
            if (sel.cursor() > other.cursor())
                sel = other;
            break;
        case CombineOp::SelectRightmostCursor:
            if (sel.cursor() < other.cursor())
                sel = other;
            break;
        case CombineOp::SelectLongest:
            if (char_length(buffer, sel) < char_length(buffer, other))
                sel = other;
            break;
        case CombineOp::SelectShortest:
            if (char_length(buffer, sel) > char_length(buffer, other))
                sel = other;
            break;
        default: kak_assert(false);
    }
}

template<typename Func>
void combine_selections(Context& context, SelectionList list, Func func)
{
    if (&context.buffer() != &list.buffer())
        throw runtime_error{"cannot combine selections from different buffers"};

    on_next_key_with_autoinfo(context, KeymapMode::None,
                             [func, list](Key key, Context& context) mutable {
                                 if (key == Key::Escape)
                                     return;

                                 const auto op = key_to_combine_op(key);
                                 auto& sels = context.selections();
                                 list.update();
                                 if (op == CombineOp::Append)
                                 {
                                     const auto main_index = list.size() + sels.main_index();
                                     for (auto& sel : sels)
                                         list.push_back(sel);
                                     list.set_main_index(main_index);
                                     list.sort_and_merge_overlapping();
                                 }
                                 else
                                 {
                                     if (list.size() != sels.size())
                                         throw runtime_error{format("the two selection lists don't have the same number of elements ({} vs {})",
                                                                    list.size(), sels.size())};
                                     for (int i = 0; i < list.size(); ++i)
                                         combine_selection(sels.buffer(), list[i], sels[i], op);
                                     list.set_main_index(sels.main_index());
                                 }
                                 func(context, std::move(list));
                             }, "enter combining operator",
                             "'a': append lists\n"
                             "'u': union\n"
                             "'i': intersection\n"
                             "'<': select leftmost cursor\n"
                             "'>': select rightmost cursor\n"
                             "'+': select longest\n"
                             "'-': select shortest\n");
}

template<bool combine>
void save_selections(Context& context, NormalParams params)
{
    const char reg = to_lower(params.reg ? params.reg : '^');
    if (not is_basic_alpha(reg) and reg != '^')
        throw runtime_error("selections can only be saved to the '^' and alphabetic registers");

    auto content = RegisterManager::instance()[reg].get(context);
    const bool empty = content.size() == 1 and content[0].empty();

    auto save_to_reg = [reg](Context& context, const SelectionList& sels) {
        auto& buffer = context.buffer();
        auto descs = concatenated(ConstArrayView<String>{format("{}@{}@{}", buffer.name(), buffer.timestamp(), sels.main_index())},
                                  sels | transform(selection_to_string)) | gather<Vector<String>>();
        RegisterManager::instance()[reg].set(context, descs);

        context.print_status({format("{} {} selections to register '{}'",
                                     combine ? "Combined" : "Saved", sels.size(), reg),
                              context.faces()["Information"]});
    };

    if (combine and not empty)
        combine_selections(context, read_selections_from_register(reg, context), save_to_reg);
    else
        save_to_reg(context, context.selections());
}

template<bool combine>
void restore_selections(Context& context, NormalParams params)
{
    const char reg = to_lower(params.reg ? params.reg : '^');
    auto selections = read_selections_from_register(reg, context);

    auto set_selections = [reg](Context& context, SelectionList sels) {
        auto size = sels.size();
        context.selections_write_only() = std::move(sels);
        context.print_status({format("{} {} selections from register '{}'",
                                     combine ? "Combined" : "Restored", size, reg),
                              context.faces()["Information"]});
    };

    if (not combine)
    {
        if (&selections.buffer() != &context.buffer())
            context.change_buffer(selections.buffer());
        set_selections(context, std::move(selections));
    }
    else
        combine_selections(context, std::move(selections), set_selections);
}

void undo(Context& context, NormalParams params)
{
    Buffer& buffer = context.buffer();
    size_t timestamp = buffer.timestamp();
    if (buffer.undo(std::max(1, params.count)))
    {
        auto ranges = compute_modified_ranges(buffer, timestamp);
        if (not ranges.empty())
            context.selections_write_only() = std::move(ranges);
    }
    else
        throw runtime_error("nothing left to undo");
}

void redo(Context& context, NormalParams params)
{
    Buffer& buffer = context.buffer();
    size_t timestamp = buffer.timestamp();
    if (buffer.redo(std::max(1, params.count)))
    {
        auto ranges = compute_modified_ranges(buffer, timestamp);
        if (not ranges.empty())
            context.selections_write_only() = std::move(ranges);
    }
    else
        throw runtime_error("nothing left to redo");
}

template<Direction direction>
void move_in_history(Context& context, NormalParams params)
{
    Buffer& buffer = context.buffer();
    size_t timestamp = buffer.timestamp();
    const int count = std::max(1, params.count);
    const int history_id = (size_t)buffer.current_history_id() + direction * count;
    const int max_history_id = (int)buffer.next_history_id() - 1;
    if (buffer.move_to((Buffer::HistoryId)history_id))
    {
        auto ranges = compute_modified_ranges(buffer, timestamp);
        if (not ranges.empty())
            context.selections_write_only() = std::move(ranges);

        context.print_status({ format("moved to change #{} ({})",
                               history_id, max_history_id),
                               context.faces()["Information"] });
    }
    else
        throw runtime_error(format("no such change: #{} ({})",
                            history_id, max_history_id));
}

void exec_user_mappings(Context& context, NormalParams params)
{
    on_next_key_with_autoinfo(context, KeymapMode::None,
                             [params](Key key, Context& context) mutable {
        if (not context.keymaps().is_mapped(key, KeymapMode::User))
            return;

        auto& mapping = context.keymaps().get_mapping(key, KeymapMode::User);
        ScopedSetBool disable_keymaps(context.keymaps_disabled());

        InputHandler::ScopedForceNormal force_normal{context.input_handler(), params};

        ScopedEdition edition(context);
        for (auto& key : mapping.keys)
            context.input_handler().handle_key(key);
    }, "user mapping",
    build_autoinfo_for_mapping(context, KeymapMode::User, {}));
}

template<bool above>
void add_empty_line(Context& context, NormalParams params)
{
    int count = std::max(params.count, 1);
    String new_lines{'\n', CharCount{count}};
    auto& buffer = context.buffer();
    auto& sels = context.selections();
    ScopedEdition edition{context};
    for (int i = 0; i < sels.size(); ++i)
    {
        auto line = (above ? sels[i].min().line : sels[i].max().line + 1) + (i * count);
        buffer.insert(line, new_lines);
    }
}

template<typename T>
class Repeated
{
public:
    constexpr Repeated(T t) : m_func(t) {}

    void operator() (Context& context, NormalParams params)
    {
        ScopedEdition edition(context);
        do { m_func(context, {0, params.reg}); } while(--params.count > 0);
    }
private:
    T m_func;
};

template<void (*func)(Context&, NormalParams)>
void repeated(Context& context, NormalParams params)
{
    ScopedEdition edition(context);
    do { func(context, {0, params.reg}); } while(--params.count > 0);
}

template<typename Type, Direction direction, SelectMode mode = SelectMode::Replace>
void move_cursor(Context& context, NormalParams params)
{
    kak_assert(mode == SelectMode::Replace or mode == SelectMode::Extend);
    const Type offset{direction * std::max(params.count,1)};
    const ColumnCount tabstop = context.options()["tabstop"].get<int>();
    auto& selections = context.selections();
    for (auto& sel : selections)
    {
        auto cursor = context.buffer().offset_coord(sel.cursor(), offset, tabstop, true);
        sel.anchor() = mode == SelectMode::Extend ? sel.anchor() : cursor;
        sel.cursor() = cursor;
    }
    selections.sort_and_merge_overlapping();
}

void select_whole_buffer(Context& context, NormalParams)
{
    select_buffer(context.selections());
}

void keep_selection(Context& context, NormalParams p)
{
    auto& selections = context.selections();
    const int index = p.count ? p.count-1 : selections.main_index();
    if (index >= selections.size())
        throw runtime_error{format("invalid selection index: {}", index)};

    selections = SelectionList{ selections.buffer(), std::move(selections[index]) };
    selections.check_invariant();
}

void remove_selection(Context& context, NormalParams p)
{
    auto& selections = context.selections();
    const int index = p.count ? p.count-1 : selections.main_index();
    if (index >= selections.size())
        throw runtime_error{format("invalid selection index: {}", index)};
    if (selections.size() == 1)
        throw runtime_error{"cannot remove the last selection"};

    selections.remove(index);
    selections.check_invariant();
}

void clear_selections(Context& context, NormalParams)
{
    for (auto& sel : context.selections())
        sel.anchor() = sel.cursor();
}

void flip_selections(Context& context, NormalParams)
{
    for (auto& sel : context.selections())
    {
        const BufferCoord tmp = sel.anchor();
        sel.anchor() = sel.cursor();
        sel.cursor() = tmp;
    }
    context.selections().check_invariant();
}

void ensure_forward(Context& context, NormalParams)
{
    for (auto& sel : context.selections())
    {
        const BufferCoord min = sel.min(), max = sel.max();
        sel.anchor() = min;
        sel.cursor() = max;
    }
    context.selections().check_invariant();
}

void merge_consecutive(Context& context, NormalParams params)
{
    ensure_forward(context, params);
    context.selections().merge_consecutive();
}

void force_redraw(Context& context, NormalParams)
{
    if (context.has_client())
    {
        context.client().force_redraw();
        context.client().redraw_ifn();
    }
}

template<typename T, MemoryDomain domain>
using KeymapBackend = Vector<T, domain>;

static const HashMap<Key, NormalCmd, MemoryDomain::Undefined, KeymapBackend> keymap = {
    { {'h'}, {"move left", move_cursor<CharCount, Backward>} },
    { {'j'}, {"move down", move_cursor<LineCount, Forward>} },
    { {'k'}, {"move up",  move_cursor<LineCount, Backward>} },
    { {'l'}, {"move right", move_cursor<CharCount, Forward>} },

    { {Key::Left}, { "move left", move_cursor<CharCount, Backward>} },
    { {Key::Down}, { "move down", move_cursor<LineCount, Forward>} },
    { {Key::Up}, {   "move up", move_cursor<LineCount, Backward>} },
    { {Key::Right}, {"move right", move_cursor<CharCount, Forward>} },

    { {'H'}, {"extend left", move_cursor<CharCount, Backward, SelectMode::Extend>} },
    { {'J'}, {"extend down", move_cursor<LineCount, Forward, SelectMode::Extend>} },
    { {'K'}, {"extend up", move_cursor<LineCount, Backward, SelectMode::Extend>} },
    { {'L'}, {"extend right", move_cursor<CharCount, Forward, SelectMode::Extend>} },

    { shift(Key::Left), {"extend left", move_cursor<CharCount, Backward, SelectMode::Extend>} },
    { shift(Key::Down), {"extend down", move_cursor<LineCount, Forward, SelectMode::Extend>} },
    { shift(Key::Up), {"extend up", move_cursor<LineCount, Backward, SelectMode::Extend>} },
    { shift(Key::Right), {"extend right", move_cursor<CharCount, Forward, SelectMode::Extend>} },

    { {'t'}, {"select to next character", select_to_next_char<SelectFlags::None>} },
    { {'f'}, {"select to next character included", select_to_next_char<SelectFlags::Inclusive>} },
    { {'T'}, {"extend to next character", select_to_next_char<SelectFlags::Extend>} },
    { {'F'}, {"extend to next character included", select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend>} },
    { {alt('t')}, {"select to previous character", select_to_next_char<SelectFlags::Reverse>} },
    { {alt('f')}, {"select to previous character included", select_to_next_char<SelectFlags::Inclusive | SelectFlags::Reverse>} },
    { {alt('T')}, {"extend to previous character", select_to_next_char<SelectFlags::Extend | SelectFlags::Reverse>} },
    { {alt('F')}, {"extend to previous character included", select_to_next_char<SelectFlags::Inclusive | SelectFlags::Extend | SelectFlags::Reverse>} },

    { {'d'}, {"erase selected text", erase_selections<true>} },
    { {alt('d')}, {"erase selected text, without yanking", erase_selections<false>} },
    { {'c'}, {"change selected text", change<true>} },
    { {alt('c')}, {"change selected text, without yanking", change<false>} },
    { {'i'}, {"insert before selected text", enter_insert_mode<InsertMode::Insert>} },
    { {'I'}, {"insert at line begin", enter_insert_mode<InsertMode::InsertAtLineBegin>} },
    { {'a'}, {"insert after selected text", enter_insert_mode<InsertMode::Append>} },
    { {'A'}, {"insert at line end", enter_insert_mode<InsertMode::AppendAtLineEnd>} },
    { {'o'}, {"insert on new line below", enter_insert_mode<InsertMode::OpenLineBelow>} },
    { {'O'}, {"insert on new line above", enter_insert_mode<InsertMode::OpenLineAbove>} },
    { {'r'}, {"replace with character", replace_with_char} },

    { {alt('o')}, {"add a new empty line below", add_empty_line<false>} },
    { {alt('O')}, {"add a new empty line above", add_empty_line<true>} },

    { {'g'}, {"go to location", goto_commands<SelectMode::Replace>} },
    { {'G'}, {"extend to location", goto_commands<SelectMode::Extend>} },

    { {'v'}, {"move view", view_commands<false>} },
    { {'V'}, {"move view (locked)", view_commands<true>} },

    { {'y'}, {"yank selected text", yank} },
    { {'p'}, {"paste after selected text", repeated<paste<InsertMode::Append>>} },
    { {'P'}, {"paste before selected text", repeated<paste<InsertMode::Insert>>} },
    { {alt('p')}, {"paste every yanked selection after selected text", paste_all<InsertMode::Append>} },
    { {alt('P')}, {"paste every yanked selection before selected text", paste_all<InsertMode::Insert>} },
    { {'R'}, {"replace selected text with yanked text", paste<InsertMode::Replace>} },
    { {alt('R')}, {"replace selected text with every yanked text", paste_all<InsertMode::Replace>} },

    { {'s'}, {"select regex matches in selected text", select_regex} },
    { {'S'}, {"split selected text on regex matches", split_regex} },
    { {alt('s')}, {"split selected text on line ends", split_lines} },
    { {alt('S')}, {"select selection boundaries", select_boundaries} },

    { {'.'}, {"repeat last insert command", repeat_last_insert} },
    { {alt('.')}, {"repeat last object select/character find", repeat_last_select} },

    { {'%'}, {"select whole buffer", select_whole_buffer} },

    { {':'}, {"enter command prompt", command} },
    { {'|'}, {"pipe each selection through filter and replace with output", pipe<true>} },
    { {alt('|')}, {"pipe each selection through command and ignore output", pipe<false>} },
    { {'!'}, {"insert command output", insert_output<InsertMode::Insert>} },
    { {alt('!')}, {"append command output", insert_output<InsertMode::Append>} },

    { {' '}, {"remove all selections except main", keep_selection} },
    { {alt(' ')}, {"remove main selection", remove_selection} },
    { {';'}, {"reduce selections to their cursor", clear_selections} },
    { {alt(';')}, {"swap selections cursor and anchor", flip_selections} },
    { {alt(':')}, {"ensure selection cursor is after anchor", ensure_forward} },
    { {alt('_')}, {"merge consecutive selections", merge_consecutive} },

    { {'w'}, {"select to next word start", repeated<&select<SelectMode::Replace, select_to_next_word<Word>>>} },
    { {'e'}, {"select to next word end", repeated<select<SelectMode::Replace, select_to_next_word_end<Word>>>} },
    { {'b'}, {"select to previous word start", repeated<select<SelectMode::Replace, select_to_previous_word<Word>>>} },
    { {'W'}, {"extend to next word start", repeated<select<SelectMode::Extend, select_to_next_word<Word>>>} },
    { {'E'}, {"extend to next word end", repeated<select<SelectMode::Extend, select_to_next_word_end<Word>>>} },
    { {'B'}, {"extend to previous word start", repeated<select<SelectMode::Extend, select_to_previous_word<Word>>>} },

    { {alt('w')}, {"select to next WORD start", repeated<select<SelectMode::Replace, select_to_next_word<WORD>>>} },
    { {alt('e')}, {"select to next WORD end", repeated<select<SelectMode::Replace, select_to_next_word_end<WORD>>>} },
    { {alt('b')}, {"select to previous WORD start", repeated<select<SelectMode::Replace, select_to_previous_word<WORD>>>} },
    { {alt('W')}, {"extend to next WORD start", repeated<select<SelectMode::Extend, select_to_next_word<WORD>>>} },
    { {alt('E')}, {"extend to next WORD end", repeated<select<SelectMode::Extend, select_to_next_word_end<WORD>>>} },
    { {alt('B')}, {"extend to previous WORD start", repeated<select<SelectMode::Extend, select_to_previous_word<WORD>>>} },

    { {alt('l')}, {"select to line end", repeated<select<SelectMode::Replace, select_to_line_end<false>>>} },
    { {Key::End}, {"select to line end", repeated<select<SelectMode::Replace, select_to_line_end<false>>>} },
    { {alt('L')}, {"extend to line end", repeated<select<SelectMode::Extend, select_to_line_end<false>>>} },
    { shift(Key::End), {"extend to line end", repeated<select<SelectMode::Extend, select_to_line_end<false>>>} },
    { {alt('h')}, {"select to line begin", repeated<select<SelectMode::Replace, select_to_line_begin<false>>>} },
    { {Key::Home}, {"select to line begin", repeated<select<SelectMode::Replace, select_to_line_begin<false>>>} },
    { {alt('H')}, {"extend to line begin", repeated<select<SelectMode::Extend, select_to_line_begin<false>>>} },
    { shift(Key::Home), {"extend to line begin", repeated<select<SelectMode::Extend, select_to_line_begin<false>>>} },

    { {'x'}, {"select line", repeated<select<SelectMode::Replace, select_line>>} },
    { {'X'}, {"extend line", repeated<select<SelectMode::Extend, select_line>>} },
    { {alt('x')}, {"extend selections to whole lines", select<SelectMode::Replace, select_lines>} },
    { {alt('X')}, {"crop selections to whole lines", select<SelectMode::Replace, trim_partial_lines>} },

    { {'m'}, {"select to matching character", select<SelectMode::Replace, select_matching<true>>} },
    { {alt('m')}, {"backward select to matching character", select<SelectMode::Replace, select_matching<false>>} },
    { {'M'}, {"extend to matching character", select<SelectMode::Extend, select_matching<true>>} },
    { {alt('M')}, {"backward extend to matching character", select<SelectMode::Extend, select_matching<false>>} },

    { {'/'}, {"select next given regex match", search<SelectMode::Replace, RegexMode::Forward>} },
    { {'?'}, {"extend with next given regex match", search<SelectMode::Extend, RegexMode::Forward>} },
    { {alt('/')}, {"select previous given regex match", search<SelectMode::Replace, RegexMode::Backward>} },
    { {alt('?')}, {"extend with previous given regex match", search<SelectMode::Extend, RegexMode::Backward>} },
    { {'n'}, {"select next current search pattern match", search_next<SelectMode::Replace, RegexMode::Forward>} },
    { {'N'}, {"extend with next current search pattern match", search_next<SelectMode::Append, RegexMode::Forward>} },
    { {alt('n')}, {"select previous current search pattern match", search_next<SelectMode::Replace, RegexMode::Backward>} },
    { {alt('N')}, {"extend with previous current search pattern match", search_next<SelectMode::Append, RegexMode::Backward>} },
    { {'*'}, {"set search pattern to main selection content", use_selection_as_search_pattern<true>} },
    { {alt('*')}, {"set search pattern to main selection content, do not detect words", use_selection_as_search_pattern<false>} },

    { {'u'}, {"undo", undo} },
    { {'U'}, {"redo", redo} },
    { {alt('u')}, {"move backward in history", move_in_history<Direction::Backward>} },
    { {alt('U')}, {"move forward in history", move_in_history<Direction::Forward>} },

    { {alt('i')}, {"select inner object", select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd | ObjectFlags::Inner>} },
    { {alt('a')}, {"select whole object", select_object<ObjectFlags::ToBegin | ObjectFlags::ToEnd>} },
    { {'['}, {"select to object start", select_object<ObjectFlags::ToBegin>} },
    { {']'}, {"select to object end", select_object<ObjectFlags::ToEnd>} },
    { {'{'}, {"extend to object start", select_object<ObjectFlags::ToBegin, SelectMode::Extend>} },
    { {'}'}, {"extend to object end", select_object<ObjectFlags::ToEnd, SelectMode::Extend>} },
    { {alt('[')}, {"select to inner object start", select_object<ObjectFlags::ToBegin | ObjectFlags::Inner>} },
    { {alt(']')}, {"select to inner object end", select_object<ObjectFlags::ToEnd | ObjectFlags::Inner>} },
    { {alt('{')}, {"extend to inner object start", select_object<ObjectFlags::ToBegin | ObjectFlags::Inner, SelectMode::Extend>} },
    { {alt('}')}, {"extend to inner object end", select_object<ObjectFlags::ToEnd | ObjectFlags::Inner, SelectMode::Extend>} },

    { {alt('j')}, {"join lines", join_lines} },
    { {alt('J')}, {"join lines and select spaces", join_lines_select_spaces} },

    { {alt('k')}, {"keep selections matching given regex", keep<true>} },
    { {alt('K')}, {"keep selections not matching given regex", keep<false>} },
    { {'$'}, {"pipe each selection through shell command and keep the ones whose command succeed", keep_pipe} },

    { {'<'}, {"deindent", deindent<true>} },
    { {'>'}, {"indent", indent<false>} },
    { {alt('>')}, {"indent, including empty lines", indent<true>} },
    { {alt('<')}, {"deindent, not including incomplete indent", deindent<false>} },

    { {/*ctrl('i')*/Key::Tab}, {"jump forward in jump list",jump<Forward>} }, // until we can distinguish tab a ctrl('i')
    { {ctrl('o')}, {"jump backward in jump list", jump<Backward>} },
    { {ctrl('s')}, {"push current selections in jump list", push_selections} },

    { {')'}, {"rotate main selection forward", rotate_selections<Forward>} },
    { {'('}, {"rotate main selection backward", rotate_selections<Backward>} },
    { {alt(')')}, {"rotate selections content forward", rotate_selections_content<Forward>} },
    { {alt('(')}, {"rotate selections content backward", rotate_selections_content<Backward>} },

    { {'q'}, {"replay recorded macro", replay_macro} },
    { {'Q'}, {"start or end macro recording", start_or_end_macro_recording} },

    { {Key::Escape}, {"end macro recording", end_macro_recording} },

    { {'`'}, {"convert to lower case in selections", for_each_codepoint<to_lower>} },
    { {'~'}, {"convert to upper case in selections", for_each_codepoint<to_upper>} },
    { {alt('`')}, { "swap case in selections", for_each_codepoint<swap_case>} },

    { {'&'}, {"align selection cursors", align} },
    { {alt('&')}, {"copy indentation", copy_indent} },

    { {'@'}, {"convert tabs to spaces in selections", tabs_to_spaces} },
    { {alt('@')}, {"convert spaces to tabs in selections", spaces_to_tabs} },

    { {'_'}, {"trim selections", trim_selections} },

    { {'C'}, {"copy selection on next lines", copy_selections_on_next_lines<Forward>} },
    { {alt('C')}, {"copy selection on previous lines", copy_selections_on_next_lines<Backward>} },

    { {','}, {"user mappings", exec_user_mappings} },

    { {Key::PageUp}, {  "scroll one page up", scroll<Backward>} },
    { {Key::PageDown}, {"scroll one page down", scroll<Forward>} },

    { {ctrl('b')}, {"scroll one page up", scroll<Backward >} },
    { {ctrl('f')}, {"scroll one page down", scroll<Forward>} },
    { {ctrl('u')}, {"scroll half a page up", scroll<Backward, true>} },
    { {ctrl('d')}, {"scroll half a page down", scroll<Forward, true>} },

    { {'z'}, {"restore selections from register", restore_selections<false>} },
    { {alt('z')}, {"combine selections from register", restore_selections<true>} },
    { {'Z'}, {"save selections to register", save_selections<false>} },
    { {alt('Z')}, {"combine selections to register", save_selections<true>} },

    { {ctrl('l')}, {"force redraw", force_redraw} },
};

Optional<NormalCmd> get_normal_command(Key key)
{
    auto it = keymap.find(key);
    if (it != keymap.end())
        return it->value;
    return {};
}

}
