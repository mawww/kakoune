#include "window.hh"
#include "buffer.hh"
#include "file.hh"
#include "command_manager.hh"
#include "buffer_manager.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "assert.hh"
#include "debug.hh"
#include "highlighters.hh"
#include "highlighter_registry.hh"
#include "filters.hh"
#include "filter_registry.hh"
#include "hooks_manager.hh"
#include "context.hh"
#include "keys.hh"

#include <unordered_map>
#include <map>
#include <sstream>
#include <boost/regex.hpp>
#include <ncurses.h>
#include <sys/types.h>
#include <sys/wait.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define CTRL(x) x - 'a' + 1

using namespace Kakoune;
using namespace std::placeholders;

void set_attribute(int attribute, bool on)
{
    if (on)
        attron(attribute);
    else
        attroff(attribute);
}

int nc_color(Color color)
{
    switch (color)
    {
    case Color::Black:   return COLOR_BLACK;
    case Color::Red:     return COLOR_RED;
    case Color::Green:   return COLOR_GREEN;
    case Color::Yellow:  return COLOR_YELLOW;
    case Color::Blue:    return COLOR_BLUE;
    case Color::Magenta: return COLOR_MAGENTA;
    case Color::Cyan:    return COLOR_CYAN;
    case Color::White:   return COLOR_WHITE;

    case Color::Default:
    default:
        return COLOR_BLACK;
    }
}

void set_color(Color fg_color, Color bg_color)
{
    static std::map<std::pair<Color, Color>, int> colorpairs;
    static int current_pair = -1;
    static int next_pair = 1;

    if (current_pair != -1)
        attroff(COLOR_PAIR(current_pair));

    if (fg_color == Color::Default and bg_color == Color::Default)
        return;

    std::pair<Color, Color> colorpair(fg_color, bg_color);
    auto it = colorpairs.find(colorpair);
      if (it != colorpairs.end())
    {
        current_pair = it->second;
        attron(COLOR_PAIR(it->second));
    }
    else
    {
        init_pair(next_pair, nc_color(fg_color), nc_color(bg_color));
        colorpairs[colorpair] = next_pair;
        current_pair = next_pair;
        attron(COLOR_PAIR(next_pair));
        ++next_pair;
    }
}

void draw_window(Window& window)
{
    int max_x,max_y;
    getmaxyx(stdscr, max_y, max_x);
    max_y -= 1;

    window.set_dimensions(DisplayCoord(max_y, max_x));
    window.update_display_buffer();

    DisplayCoord position;
    for (const DisplayAtom& atom : window.display_buffer())
    {
        assert(position == atom.coord());
        const std::string content = atom.content();

        set_attribute(A_UNDERLINE, atom.attribute() & Underline);
        set_attribute(A_REVERSE, atom.attribute() & Reverse);
        set_attribute(A_BLINK, atom.attribute() & Blink);
        set_attribute(A_BOLD, atom.attribute() & Bold);

        set_color(atom.fg_color(), atom.bg_color());

        size_t pos = 0;
        size_t end;
        while (true)
        {
            move(position.line, position.column);
            clrtoeol();
            end = content.find_first_of('\n', pos);
            std::string line = content.substr(pos, end - pos);
            addstr(line.c_str());

            if (end != std::string::npos)
            {
                addch(' ');
                position.line = position.line + 1;
                position.column = 0;
                pos = end + 1;

                if (position.line >= max_y)
                    break;
            }
            else
            {
                position.column += line.length();
                break;
            }
        }
        if (position.line >= max_y)
            break;
    }

    set_attribute(A_UNDERLINE, 0);
    set_attribute(A_REVERSE, 0);
    set_attribute(A_BLINK, 0);
    set_attribute(A_BOLD, 0);
    set_color(Color::Blue, Color::Black);
    while (++position.line < max_y)
    {
        move(position.line, 0);
        clrtoeol();
        addch('~');
    }

    set_color(Color::Cyan, Color::Black);
    std::string status_line = window.status_line();
    static int last_status_length = 0;
    move(max_y, max_x - last_status_length);
    clrtoeol();
    move(max_y, max_x - status_line.length());
    addstr(status_line.c_str());
    last_status_length = status_line.length();

    const DisplayCoord& cursor_position = window.cursor_position();
    move(cursor_position.line, cursor_position.column);
}

void draw_editor_ifn(Editor& editor)
{
    Window* window = dynamic_cast<Window*>(&editor);
    if (window)
        draw_window(*window);
}

Key ncurses_get_key()
{
    char c = getch();

    Key::Modifiers modifiers = Key::Modifiers::None;
    if (c > 0 and c < 27)
    {
        modifiers = Key::Modifiers::Control;
        c = c - 1 + 'a';
    }
    else if (c == 27)
    {
        timeout(0);
        char new_c = getch();
        timeout(-1);
        if (new_c != ERR)
        {
            c = new_c;
            modifiers = Key::Modifiers::Alt;
        }
    }
    return Key(modifiers, c);
}

void init_ncurses()
{
    initscr();
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, false);
    keypad(stdscr, true);
    curs_set(0);
    start_color();
    ESCDELAY=25;
}

void deinit_ncurses()
{
    endwin();
}

struct prompt_aborted {};

std::string ncurses_prompt(const std::string& text, Completer completer = complete_nothing)
{
    curs_set(2);
    auto restore_cursor = on_scope_end([]() { curs_set(0); });

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);
    move(max_y-1, 0);
    addstr(text.c_str());
    clrtoeol();

    size_t cursor_pos = 0;

    Completions completions;
    int current_completion = -1;
    std::string text_before_completion;

    std::string result;
    std::string saved_result;

    static std::unordered_map<std::string, std::vector<std::string>> history_per_prompt;
    std::vector<std::string>& history = history_per_prompt[text];
    auto history_it = history.end();

    while (true)
    {
        int c = getch();
        switch (c)
        {
        case '\r':
        {
            std::vector<std::string>::iterator it;
            while ((it = find(history, result)) != history.end())
                history.erase(it);

            history.push_back(result);
            return result;
        }
        case KEY_UP:
            if (history_it != history.begin())
            {
                if (history_it == history.end())
                   saved_result = result;
                --history_it;
                result = *history_it;
                cursor_pos = result.length();
            }
            break;
        case KEY_DOWN:
            if (history_it != history.end())
            {
                ++history_it;
                if (history_it != history.end())
                    result = *history_it;
                else
                    result = saved_result;
                cursor_pos = result.length();
            }
            break;
        case KEY_LEFT:
            if (cursor_pos > 0)
                --cursor_pos;
            break;
        case KEY_RIGHT:
            if (cursor_pos < result.length())
                ++cursor_pos;
            break;
        case KEY_BACKSPACE:
            if (cursor_pos != 0)
            {
                result = result.substr(0, cursor_pos - 1)
                       + result.substr(cursor_pos, std::string::npos);

                --cursor_pos;
            }

            current_completion = -1;
            break;
        case CTRL('r'):
            {
                c = getch();
                std::string reg = RegisterManager::instance()[c].get();
                current_completion = -1;
                result = result.substr(0, cursor_pos) + reg + result.substr(cursor_pos, std::string::npos);
                cursor_pos += reg.length();
            }
            break;
        case 27:
            throw prompt_aborted();
        case '\t':
        {
            if (current_completion == -1)
            {
                completions = completer(result, cursor_pos);
                if (completions.candidates.empty())
                    break;

                text_before_completion = result.substr(completions.start,
                                                       completions.end - completions.start);
            }
            ++current_completion;

            std::string completion;
            if (current_completion >= completions.candidates.size())
            {
                if (current_completion == completions.candidates.size() and
                    std::find(completions.candidates.begin(), completions.candidates.end(), text_before_completion) == completions.candidates.end())
                    completion = text_before_completion;
                else
                {
                    current_completion = 0;
                    completion = completions.candidates[0];
                }
            }
            else
                completion = completions.candidates[current_completion];

            move(max_y-1, text.length());
            result = result.substr(0, completions.start) + completion;
            cursor_pos = completions.start + completion.length();
            break;
        }
        default:
            current_completion = -1;
            result = result.substr(0, cursor_pos) + (char)c + result.substr(cursor_pos, std::string::npos);
            ++cursor_pos;
        }

        move(max_y - 1, text.length());
        clrtoeol();
        addstr(result.c_str());
        move(max_y - 1, text.length() + cursor_pos);
        refresh();
    }
    return result;
}

static std::function<std::string (const std::string&, Completer)> prompt_func  = ncurses_prompt;

std::string prompt(const std::string& text, Completer completer = complete_nothing)
{
    return prompt_func(text, completer);
}

static std::function<Key ()> get_key_func = ncurses_get_key;

Key get_key()
{
    return get_key_func();
}


void print_status(const std::string& status)
{
    int x,y;
    getmaxyx(stdscr, y, x);
    move(y-1, 0);
    clrtoeol();
    addstr(status.c_str());
}

struct InsertSequence
{
    IncrementalInserter::Mode mode;
    std::vector<Key>          keys;

    InsertSequence() : mode(IncrementalInserter::Mode::Insert) {}
};

InsertSequence last_insert_sequence;

bool insert_char(IncrementalInserter& inserter, const Key& key)
{
    switch (key.modifiers)
    {
    case Key::Modifiers::None:
        switch (key.key)
        {
        case 27:
            return false;
        default:
            inserter.insert(std::string() + key.key);
        }
        break;
    case Key::Modifiers::Control:
        switch (key.key)
        {
        case 'r':
        {
            Key next_key = get_key();
            last_insert_sequence.keys.push_back(next_key);
            if (next_key.modifiers == Key::Modifiers::None)
            {
                switch (next_key.key)
                {
                case '%':
                    inserter.insert(inserter.buffer().name());
                    break;
                default:
                    inserter.insert(RegisterManager::instance()[next_key.key]);
                }
            }
            break;
        }
        case 'm':
            inserter.insert(std::string() + '\n');
            break;
        case 'd':
            inserter.move_cursors({0, -1});
            break;
        case 'e':
            inserter.move_cursors({0,  1});
            break;
        case 'g':
            inserter.erase();
            break;
        }
        break;
    }
    return true;
}

void do_insert(Editor& editor, IncrementalInserter::Mode mode)
{
    last_insert_sequence.mode = mode;
    last_insert_sequence.keys.clear();
    IncrementalInserter inserter(editor, mode);
    draw_editor_ifn(editor);
    while(true)
    {
        Key key = get_key();

        if (not insert_char(inserter, key))
            break;

        last_insert_sequence.keys.push_back(key);
        draw_editor_ifn(editor);
    }
}

void do_repeat_insert(Editor& editor, int count)
{
    IncrementalInserter inserter(editor, last_insert_sequence.mode);
    for (const Key& key : last_insert_sequence.keys)
    {
        insert_char(inserter, key);
    }
    draw_editor_ifn(editor);
}


template<bool append>
void do_go(Editor& editor, int count)
{
    if (count != 0)
    {
        BufferIterator target =
            editor.buffer().iterator_at(BufferCoord(count-1, 0));

        editor.select(target);
    }
    else
    {
        Key key = get_key();
        if (key.modifiers != Key::Modifiers::None)
            return;

        switch (key.key)
        {
        case 'g':
        case 't':
        {
            BufferIterator target =
                editor.buffer().iterator_at(BufferCoord(0,0));
            editor.select(target);
            break;
        }
        case 'l':
        case 'L':
            editor.select(select_to_eol, append);
            break;
        case 'h':
        case 'H':
            editor.select(select_to_eol_reverse, append);
            break;
        case 'b':
        {
            BufferIterator target = editor.buffer().iterator_at(
                BufferCoord(editor.buffer().line_count() - 1, 0));
            editor.select(target);
            break;
        }
        }
    }
}

Context main_context;

Buffer* open_or_create(const std::string& filename)
{
    Buffer* buffer = NULL;
    try
    {
        buffer = create_buffer_from_file(filename);
    }
    catch (file_not_found& what)
    {
        print_status("new file " + filename);
        buffer = new Buffer(filename, Buffer::Type::NewFile);
    }
    return buffer;
}

void edit(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    std::string filename = params[0];
    main_context = Context(*open_or_create(filename)->get_or_create_window());
}

void write_buffer(const CommandParameters& params, const Context& context)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    Buffer& buffer = context.window().buffer();
    std::string filename = params.empty() ? buffer.name()
                                          : parse_filename(params[0]);

    write_buffer_to_file(buffer, filename);
    buffer.notify_saved();
}

bool quit_requested = false;

template<bool force>
void quit(const CommandParameters& params, const Context& context)
{
    if (params.size() != 0)
        throw wrong_argument_count();

    if (not force)
    {
        for (auto& buffer : BufferManager::instance())
        {
            if (buffer.type() != Buffer::Type::Scratch and buffer.is_modified())
            {
                print_status("modified buffer remaining");
                return;
            }
        }
    }
    quit_requested = true;
}

template<bool force>
void write_and_quit(const CommandParameters& params, const Context& context)
{
    write_buffer(params, context);
    quit<force>(CommandParameters(), context);
}

void show_buffer(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    Buffer* buffer = BufferManager::instance().get_buffer(params[0]);
    if (not buffer)
        print_status("buffer " + params[0] + " does not exists");
    else
        main_context = Context(*buffer->get_or_create_window());
}

void add_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 1)
        throw wrong_argument_count();

    try
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();
        HighlighterParameters highlighter_params(params.begin()+1, params.end());
        registry.add_highlighter_to_window(context.window(), params[0],
                                           highlighter_params);
    }
    catch (runtime_error& err)
    {
        print_status("error: " + err.description());
    }
}

void add_group_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 2)
        throw wrong_argument_count();

    try
    {
        HighlighterRegistry& registry = HighlighterRegistry::instance();

        HighlighterGroup& group = context.window().highlighters().get_group(params[0]);
        HighlighterParameters highlighter_params(params.begin()+2, params.end());
        registry.add_highlighter_to_group(context.window(), group,
                                          params[1], highlighter_params);
    }
    catch (runtime_error& err)
    {
        print_status("error: " + err.description());
    }
}

void rm_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    context.window().highlighters().remove(params[0]);
}

void rm_group_highlighter(const CommandParameters& params, const Context& context)
{
    if (params.size() != 2)
        throw wrong_argument_count();

    try
    {
        HighlighterGroup& group = context.window().highlighters().get_group(params[0]);
        group.remove(params[1]);
    }
    catch (runtime_error& err)
    {
        print_status("error: " + err.description());
    }
}
void add_filter(const CommandParameters& params, const Context& context)
{
    if (params.size() < 1)
        throw wrong_argument_count();

    try
    {
        FilterRegistry& registry = FilterRegistry::instance();
        FilterParameters filter_params(params.begin()+1, params.end());
        registry.add_filter_to_window(context.window(), params[0],
                                      filter_params);
    }
    catch (runtime_error& err)
    {
        print_status("error: " + err.description());
    }
}

void rm_filter(const CommandParameters& params, const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    context.window().remove_filter(params[0]);
}

void add_hook(const CommandParameters& params, const Context& context)
{
    if (params.size() < 4)
        throw wrong_argument_count();

    std::string regex = params[2];
    std::vector<std::string> hook_params(params.begin()+3, params.end());

    auto hook_func = [=](const std::string& param, const Context& context) {
        if (boost::regex_match(param, boost::regex(regex)))
            CommandManager::instance().execute(hook_params, context);
    };

    if (params[0] == "global")
        GlobalHooksManager::instance().add_hook(params[1], hook_func);
    else if (params[0] == "window")
        context.window().hooks_manager().add_hook(params[1], hook_func);
    else
        print_status("error: no such hook container " + params[0]);
}

void exec_commands_in_file(const CommandParameters& params,
                           const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    std::string file_content = read_file(parse_filename(params[0]));
    CommandManager& cmd_manager = CommandManager::instance();

    size_t pos = 0;
    size_t length = file_content.length();
    bool   cat_with_previous = false;
    std::string command_line;
    while (true)
    {
         if (not cat_with_previous)
             command_line.clear();

         size_t end_pos = pos;

         while (file_content[end_pos] != '\n' and end_pos != length)
         {
             if (file_content[end_pos] == '"' or file_content[end_pos] == '\'' or
                 file_content[end_pos] == '`')
             {
                 char delimiter = file_content[end_pos];
                 ++end_pos;
                 while (file_content[end_pos] != delimiter or
                        file_content[end_pos-1] == '\\' and end_pos != length)
                     ++end_pos;

                 if (end_pos == length)
                 {
                     print_status(std::string("unterminated '") + delimiter + "' string");
                     return;
                 }

                 ++end_pos;
             }

             ++end_pos;
         }
         if (end_pos != pos and end_pos != length and
             file_content[end_pos - 1] == '\\')
         {
 	     command_line += file_content.substr(pos, end_pos - pos - 1);
             cat_with_previous = true;
         }
         else
         {
 	     command_line += file_content.substr(pos, end_pos - pos);
 	     cmd_manager.execute(command_line, context);
             cat_with_previous = false;
 	 }
         if (end_pos == length)
         {
             if (cat_with_previous)
                 print_status("while executing commands in \"" + params[0] +
                              "\": last command not complete");
             break;
         }
         pos = end_pos + 1;
    }
}

void exec_commands_in_runtime_file(const CommandParameters& params,
                                   const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    const std::string& filename = params[0];
    char buffer[2048];
#if defined(__linux__)
    readlink("/proc/self/exe", buffer, 2048 - filename.length());
#elif defined(__APPLE__)
    uint32_t bufsize = 2048 - filename.length();
    _NSGetExecutablePath(buffer, &bufsize);
#else
# error "finding executable path is not implemented on this platform"
#endif
    char* ptr = strrchr(buffer, '/');
    if (ptr)
    {
        strcpy(ptr+1, filename.c_str());
        exec_commands_in_file(CommandParameters(buffer), main_context);
    }
}

void do_command()
{
    try
    {
        auto cmdline = prompt(":", std::bind(&CommandManager::complete,
                                             &CommandManager::instance(),
                                             _1, _2));

        CommandManager::instance().execute(cmdline, main_context);
    }
    catch (prompt_aborted&) {}
}

void do_pipe(Editor& editor, int count)
{
    try
    {
        auto cmdline = prompt("|", complete_nothing);

        editor.buffer().begin_undo_group();
        for (auto& sel : const_cast<const Editor&>(editor).selections())
        {
            int write_pipe[2];
            int read_pipe[2];

            pipe(write_pipe);
            pipe(read_pipe);

            if (pid_t pid = fork())
            {
                close(write_pipe[0]);
                close(read_pipe[1]);

                std::string content = editor.buffer().string(sel.begin(), sel.end());
                write(write_pipe[1], content.c_str(), content.size());
                close(write_pipe[1]);

                std::string new_content;
                char buffer[1024];
                while (size_t size = read(read_pipe[0], buffer, 1024))
                {
                    new_content += std::string(buffer, buffer+size);
                }
                close(read_pipe[0]);
                waitpid(pid, NULL, 0);

                editor.buffer().modify(Modification::make_erase(sel.begin(), sel.end()));
                editor.buffer().modify(Modification::make_insert(sel.begin(), new_content));
            }
            else
            {
                close(write_pipe[1]);
                close(read_pipe[0]);

                dup2(read_pipe[1], 1);
                dup2(write_pipe[0], 0);

                execlp("sh", "sh", "-c", cmdline.c_str(), NULL);
            }
        }
        editor.buffer().end_undo_group();
    }
    catch (prompt_aborted&) {}
}

void do_search(Editor& editor)
{
    try
    {
        std::string ex = prompt("/");
        if (ex.empty())
            ex = RegisterManager::instance()['/'].get();
        else
            RegisterManager::instance()['/'] = ex;

        editor.select(std::bind(select_next_match, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_search_next(Editor& editor)
{
    const std::string& ex = RegisterManager::instance()['/'].get();
    if (not ex.empty())
        editor.select(std::bind(select_next_match, _1, ex));
    else
        print_status("no search pattern");
}

void do_yank(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
}

void do_erase(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
    editor.erase();
}

void do_change(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
    do_insert(editor, IncrementalInserter::Mode::Change);
}

template<bool append>
void do_paste(Editor& editor, int count)
{
    Register& reg = RegisterManager::instance()['"'];
    if (count == 0)
    {
        if (append)
            editor.append(reg);
        else
            editor.insert(reg);
    }
    else
    {
        if (append)
            editor.append(reg.get(count-1));
        else
            editor.insert(reg.get(count-1));
    }
}

void do_select_regex(Editor& editor, int count)
{
    try
    {
        std::string ex = prompt("select: ");
        editor.multi_select(std::bind(select_all_matches, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_split_regex(Editor& editor, int count)
{
    try
    {
        std::string ex = prompt("split: ");
        editor.multi_select(std::bind(split_selection, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_join(Editor& editor, int count)
{
    editor.select(select_whole_lines);
    editor.select(select_to_eol, true);
    editor.multi_select(std::bind(select_all_matches, _1, "\n\\h*"));
    editor.replace(" ");
    editor.clear_selections();
    editor.move_selections({0, -1});
}

template<bool inside>
void do_select_surrounding(Editor& editor, int count)
{
    char id = getch();

    static const std::unordered_map<char, std::pair<char, char>> id_to_matching =
    {
       { '(', { '(', ')' } },
       { ')', { '(', ')' } },
       { 'b', { '(', ')' } },
       { '{', { '{', '}' } },
       { '}', { '{', '}' } },
       { 'B', { '{', '}' } },
       { '[', { '[', ']' } },
       { ']', { '[', ']' } },
       { '<', { '<', '>' } },
       { '>', { '<', '>' } }
    };

    auto matching = id_to_matching.find(id);
    if (matching != id_to_matching.end())
        editor.select(std::bind(select_surrounding, _1, matching->second, inside));
}

std::unordered_map<Key, std::function<void (Editor& editor, int count)>> keymap =
{
    { { Key::Modifiers::None, 'h' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0, -std::max(count,1))); } },
    { { Key::Modifiers::None, 'j' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord( std::max(count,1), 0)); } },
    { { Key::Modifiers::None, 'k' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(-std::max(count,1), 0)); } },
    { { Key::Modifiers::None, 'l' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0,  std::max(count,1))); } },

    { { Key::Modifiers::None, 'H' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0, -std::max(count,1)), true); } },
    { { Key::Modifiers::None, 'J' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord( std::max(count,1), 0), true); } },
    { { Key::Modifiers::None, 'K' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(-std::max(count,1), 0), true); } },
    { { Key::Modifiers::None, 'L' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0,  std::max(count,1)), true); } },

    { { Key::Modifiers::None, 't' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, getch(), count, false)); } },
    { { Key::Modifiers::None, 'f' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, getch(), count, true)); } },
    { { Key::Modifiers::None, 'T' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, getch(), count, false), true); } },
    { { Key::Modifiers::None, 'F' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, getch(), count, true), true); } },

    { { Key::Modifiers::None, 'd' }, do_erase },
    { { Key::Modifiers::None, 'c' }, do_change },
    { { Key::Modifiers::None, 'i' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::Insert); } },
    { { Key::Modifiers::None, 'I' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::InsertAtLineBegin); } },
    { { Key::Modifiers::None, 'a' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::Append); } },
    { { Key::Modifiers::None, 'A' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::AppendAtLineEnd); } },
    { { Key::Modifiers::None, 'o' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::OpenLineBelow); } },
    { { Key::Modifiers::None, 'O' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::OpenLineAbove); } },

    { { Key::Modifiers::None, 'g' }, do_go<false> },
    { { Key::Modifiers::None, 'G' }, do_go<true> },

    { { Key::Modifiers::None, 'y' }, do_yank },
    { { Key::Modifiers::None, 'p' }, do_paste<true> },
    { { Key::Modifiers::None, 'P' }, do_paste<false> },

    { { Key::Modifiers::None, 's' }, do_select_regex },


    { { Key::Modifiers::None, '.' }, do_repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Editor& editor, int count) { editor.clear_selections(); editor.select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, [](Editor& editor, int count) { do_command(); } },
    { { Key::Modifiers::None, '|' }, do_pipe },
    { { Key::Modifiers::None, ' ' }, [](Editor& editor, int count) { if (count == 0) editor.clear_selections();
                                                                     else editor.keep_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word); } while(--count > 0); } },
    { { Key::Modifiers::None, 'e' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end); } while(--count > 0); } },
    { { Key::Modifiers::None, 'b' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word); } while(--count > 0); } },
    { { Key::Modifiers::None, 'W' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'E' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'B' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'x' }, [](Editor& editor, int count) { do { editor.select(select_line, false); } while(--count > 0); } },
    { { Key::Modifiers::None, 'X' }, [](Editor& editor, int count) { do { editor.select(select_line, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'm' }, [](Editor& editor, int count) { editor.select(select_matching); } },
    { { Key::Modifiers::None, 'M' }, [](Editor& editor, int count) { editor.select(select_matching, true); } },
    { { Key::Modifiers::None, '/' }, [](Editor& editor, int count) { do_search(editor); } },
    { { Key::Modifiers::None, 'n' }, [](Editor& editor, int count) { do_search_next(editor); } },
    { { Key::Modifiers::None, 'u' }, [](Editor& editor, int count) { do { if (not editor.undo()) { print_status("nothing left to undo"); break; } } while(--count > 0); } },
    { { Key::Modifiers::None, 'U' }, [](Editor& editor, int count) { do { if (not editor.redo()) { print_status("nothing left to redo"); break; } } while(--count > 0); } },

    { { Key::Modifiers::Alt,  'i' }, do_select_surrounding<true> },
    { { Key::Modifiers::Alt,  'a' }, do_select_surrounding<false> },

    { { Key::Modifiers::Alt, 't' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, getch(), count, false)); } },
    { { Key::Modifiers::Alt, 'f' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, getch(), count, true)); } },
    { { Key::Modifiers::Alt, 'T' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, getch(), count, false), true); } },
    { { Key::Modifiers::Alt, 'F' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, getch(), count, true), true); } },

    { { Key::Modifiers::Alt, 'w' }, [](Editor& editor, int count) { do { editor.select(select_to_next_WORD); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'e' }, [](Editor& editor, int count) { do { editor.select(select_to_next_WORD_end); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'b' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_WORD); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'W' }, [](Editor& editor, int count) { do { editor.select(select_to_next_WORD, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'E' }, [](Editor& editor, int count) { do { editor.select(select_to_next_WORD_end, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'B' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_WORD, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 'l' }, [](Editor& editor, int count) { do { editor.select(select_to_eol, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'L' }, [](Editor& editor, int count) { do { editor.select(select_to_eol, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'h' }, [](Editor& editor, int count) { do { editor.select(select_to_eol_reverse, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'H' }, [](Editor& editor, int count) { do { editor.select(select_to_eol_reverse, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 's' }, do_split_regex },

    { { Key::Modifiers::Alt, 'j' }, do_join },

    { { Key::Modifiers::Alt, 'x' }, [](Editor& editor, int count) { editor.select(select_whole_lines); } },
};

void exec_keys(const KeyList& keys,
               const Context& context)
{
    auto prompt_save = prompt_func;
    auto get_key_save = get_key_func;

    auto restore_funcs = on_scope_end([&]() {
        prompt_func = prompt_save;
        get_key_func = get_key_save;
    });

    size_t pos = 0;

    prompt_func = [&](const std::string&, Completer) {
        size_t begin = pos;
        while (pos < keys.size() and keys[pos].key != '\n')
            ++pos;

        std::string result;
        for (size_t i = begin; i < pos; ++i)
            result += keys[i].key;
        ++pos;

        return result;
    };

    get_key_func = [&]() {
        if (pos >= keys.size())
            throw runtime_error("no more characters");
        return keys[pos++];
    };

    Editor batch_editor(context.buffer());
    Editor& editor = context.has_window() ? static_cast<Editor&>(context.window())
                                          : static_cast<Editor&>(batch_editor);


    scoped_edition edition(editor);

    int count = 0;
    while(pos < keys.size())
    {
        const Key& key = keys[pos++];

        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            count = count * 10 + key.key - '0';
        else
        {
            auto it = keymap.find(key);
            if (it != keymap.end())
                it->second(editor, count);
            count = 0;
        }
    }
}

void exec_string(const CommandParameters& params,
                 const Context& context)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    KeyList keys = parse_keys(params[0]);

    exec_keys(keys, context);
}

int main(int argc, char* argv[])
{
    init_ncurses();

    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    FilterRegistry      filter_registry;
    GlobalHooksManager  hooks_manager;

    command_manager.register_commands({ "e", "edit" }, edit,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_commands({ "q", "quit" }, quit<false>);
    command_manager.register_commands({ "q!", "quit!" }, quit<true>);
    command_manager.register_commands({ "w", "write" }, write_buffer,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_command("wq", write_and_quit<false>);
    command_manager.register_command("wq!", write_and_quit<true>);
    command_manager.register_commands({ "b", "buffer" }, show_buffer,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         std::bind(&BufferManager::complete_buffername, &buffer_manager, _1, _2)
                                      }));
    command_manager.register_commands({ "ah", "addhl" }, add_highlighter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         std::bind(&HighlighterRegistry::complete_highlighter, &highlighter_registry, _1, _2)
                                     }));
    command_manager.register_commands({ "agh", "addgrouphl" }, add_group_highlighter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const std::string& prefix, size_t cursor_pos)
                                         { return main_context.window().highlighters().complete_group_id(prefix, cursor_pos); },
                                         std::bind(&HighlighterRegistry::complete_highlighter, &highlighter_registry, _1, _2)
                                     }));
    command_manager.register_commands({ "rh", "rmhl" }, rm_highlighter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const std::string& prefix, size_t cursor_pos)
                                         { return main_context.window().highlighters().complete_group_id(prefix, cursor_pos); }
                                     }));
    command_manager.register_commands({ "rgh", "rmgrouphl" }, rm_group_highlighter,
                                     CommandManager::None,
                                     [&](const CommandParameters& params, size_t token_to_complete, size_t pos_in_token)
                                     {
                                         Window& w = main_context.window();
                                         const std::string& arg = token_to_complete < params.size() ?
                                                                  params[token_to_complete] : std::string();
                                         if (token_to_complete == 0)
                                             return w.highlighters().complete_group_id(arg, pos_in_token);
                                         else if (token_to_complete == 1)
                                             return w.highlighters().get_group(params[0]).complete_id(arg, pos_in_token);
                                     });
    command_manager.register_commands({ "af", "addfilter" }, add_filter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         std::bind(&FilterRegistry::complete_filter, &filter_registry, _1, _2)
                                     }));
    command_manager.register_commands({ "rf", "rmfilter" }, rm_filter,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({
                                         [&](const std::string& prefix, size_t cursor_pos)
                                         { return main_context.window().complete_filterid(prefix, cursor_pos); }
                                     }));
    command_manager.register_command("hook", add_hook, CommandManager::IgnoreSemiColons | CommandManager::DeferredShellEval);

    command_manager.register_command("source", exec_commands_in_file,
                                     CommandManager::None,
                                     PerArgumentCommandCompleter({ complete_filename }));
    command_manager.register_command("runtime", exec_commands_in_runtime_file);

    command_manager.register_command("exec", exec_string);

    register_highlighters();
    register_filters();

    try
    {
        exec_commands_in_runtime_file({ "kakrc" }, main_context);
    }
     catch (Kakoune::runtime_error& error)
    {
        print_status(error.description());
    }

    try
    {
        write_debug("*** This is the debug buffer, where debug info will be written ***\n");

        auto buffer = (argc > 1) ? open_or_create(argv[1]) : new Buffer("*scratch*", Buffer::Type::Scratch);
        main_context = Context(*buffer->get_or_create_window());

        draw_window(main_context.window());
        int count = 0;
        while(not quit_requested)
        {
            try
            {
                Key key = get_key();
                if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
                    count = count * 10 + key.key - '0';
                else
                {
                    auto it = keymap.find(key);
                    if (it != keymap.end())
                    {
                        it->second(main_context.window(), count);
                        draw_window(main_context.window());
                    }
                    count = 0;
                }
            }
            catch (Kakoune::runtime_error& error)
            {
                print_status(error.description());
            }
        }
        deinit_ncurses();
    }
    catch (Kakoune::exception& error)
    {
        deinit_ncurses();
        puts("uncaught exception:\n");
        puts(error.description().c_str());
        return -1;
    }
    catch (...)
    {
        deinit_ncurses();
        throw;
    }
    return 0;
}
