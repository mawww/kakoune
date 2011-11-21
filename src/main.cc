#include "window.hh"
#include "buffer.hh"
#include "file.hh"
#include "command_manager.hh"
#include "buffer_manager.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "assert.hh"
#include "debug.hh"
#include "filters.hh"
#include "filter_registry.hh"

#include <unordered_map>
#include <map>
#include <sstream>
#include <ncurses.h>

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
    move(max_y, max_x - status_line.length());
    clrtoeol();
    addstr(status_line.c_str());

    const DisplayCoord& cursor_position = window.cursor_position();
    move(cursor_position.line, cursor_position.column);
}

void init_ncurses()
{
    initscr();
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, false);
    keypad(stdscr, true);
    curs_set(2);
    start_color();
    ESCDELAY=25;
}

void deinit_ncurses()
{
    endwin();
}

struct prompt_aborted {};

std::string prompt(const std::string& text, Completer completer = complete_nothing)
{
    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);
    move(max_y-1, 0);
    addstr(text.c_str());
    clrtoeol();

    std::string result;
    size_t cursor_pos = 0;

    Completions completions;
    int current_completion = -1;
    std::string text_before_completion;

    while (true)
    {
        char c = getch();
        switch (c)
        {
        case '\r':
            return result;
        case 4:
            if (cursor_pos > 0)
                --cursor_pos;
            break;
        case 5:
            if (cursor_pos < result.length())
                ++cursor_pos;
            break;
        case 7:
            if (cursor_pos != 0)
            {
                result = result.substr(0, cursor_pos - 1)
                       + result.substr(cursor_pos, std::string::npos);

                --cursor_pos;
            }

            current_completion = -1;
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
            result = result.substr(0, cursor_pos) + c + result.substr(cursor_pos, std::string::npos);
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

void print_status(const std::string& status)
{
    int x,y;
    getmaxyx(stdscr, y, x);
    move(y-1, 0);
    clrtoeol();
    addstr(status.c_str());
}

void do_insert(Window& window, IncrementalInserter::Mode mode)
{
    Kakoune::IncrementalInserter inserter(window, mode);
    draw_window(window);
    while(true)
    {
        const DisplayCoord& pos = window.cursor_position();
        move(pos.line, pos.column);

        char c = getch();
        switch (c)
        {
        case 27:
            return;

        case 2:
            c = getch();
            if (c >= '0' and c <= '9')
                inserter.insert_capture(c - '0');
            break;

        case 4:
            inserter.move_cursor({0, -1});
            break;
        case 5:
            inserter.move_cursor({0,  1});
            break;

        case 7:
            inserter.erase();
            break;

        case '\r':
            c = '\n';
        default:
            inserter.insert(std::string() + c);
        }
        draw_window(window);
    }
    window.clear_selections();
}

template<bool append>
void do_go(Window& window, int count)
{
    if (count != 0)
    {
        BufferIterator target =
            window.buffer().iterator_at(BufferCoord(count-1, 0));

        window.move_cursor_to(target);
    }
    else
    {
        char c = getch();
        switch (c)
        {
        case 'g':
        case 't':
        {
            BufferIterator target =
                window.buffer().iterator_at(BufferCoord(0,0));
            window.move_cursor_to(target);
            break;
        }
        case 'l':
        case 'L':
            window.select(select_to_eol, append);
            break;
        case 'h':
        case 'H':
            window.select(select_to_eol_reverse, append);
            break;
        case 'b':
        {
            BufferIterator target = window.buffer().iterator_at(
                BufferCoord(window.buffer().line_count() - 1, 0));
            window.move_cursor_to(target);
            break;
        }
        }
    }
}

Window* current_window;

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
        buffer = new Buffer(filename, Buffer::Type::File);
    }
    return buffer;
}

void edit(const CommandParameters& params)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    std::string filename = params[0];
    current_window = open_or_create(filename)->get_or_create_window();
}

void write_buffer(const CommandParameters& params)
{
    if (params.size() > 1)
        throw wrong_argument_count();

    Buffer& buffer = current_window->buffer();
    std::string filename = params.empty() ? buffer.name() : params[0];

    write_buffer_to_file(buffer, filename);
    buffer.notify_saved();
}

bool quit_requested = false;

template<bool force>
void quit(const CommandParameters& params)
{
    if (params.size() != 0)
        throw wrong_argument_count();

    if (not force)
    {
        for (auto& buffer : BufferManager::instance())
        {
            if (buffer.type() == Buffer::Type::File and buffer.is_modified())
            {
                print_status("modified buffer remaining");
                return;
            }
        }
    }
    quit_requested = true;
}

void show_buffer(const CommandParameters& params)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    Buffer* buffer = BufferManager::instance().get_buffer(params[0]);
    if (not buffer)
        print_status("buffer " + params[0] + " does not exists");
    else
        current_window = buffer->get_or_create_window();
}

void add_filter(const CommandParameters& params)
{
    if (params.size() < 1)
        throw wrong_argument_count();

    try
    {
        FilterRegistry& registry = FilterRegistry::instance();
        FilterParameters filter_params(params.begin()+1, params.end());
        registry.add_filter_to_window(*current_window, params[0],
                                      filter_params);
    }
    catch (runtime_error& err)
    {
        print_status("error: " + err.description());
    }
}

void rm_filter(const CommandParameters& params)
{
    if (params.size() != 1)
        throw wrong_argument_count();

    current_window->remove_filter(params[0]);
}

void do_command()
{
    try
    {
        auto cmdline = prompt(":", std::bind(&CommandManager::complete,
                                             &CommandManager::instance(),
                                             _1, _2));

        CommandManager::instance().execute(cmdline);
    }
    catch (prompt_aborted&) {}
}

void do_search(Window& window)
{
    try
    {
        std::string ex = prompt("/");
        if (ex.empty())
            ex = RegisterManager::instance()['/'];
        else
            RegisterManager::instance()['/'] = ex;

        window.select(std::bind(select_next_match, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_search_next(Window& window)
{
    std::string& ex = RegisterManager::instance()['/'];
    if (not ex.empty())
        window.select(std::bind(select_next_match, _1, ex));
    else
        print_status("no search pattern");
}

void do_yank(Window& window, int count)
{
    RegisterManager::instance()['"'] = window.selection_content();
    window.clear_selections();
}

void do_erase(Window& window, int count)
{
    RegisterManager::instance()['"'] = window.selection_content();
    window.erase();
    window.clear_selections();
}

void do_change(Window& window, int count)
{
    RegisterManager::instance()['"'] = window.selection_content();
    do_insert(window, IncrementalInserter::Mode::Change);
    window.clear_selections();
}

template<bool append>
void do_paste(Window& window, int count)
{
    if (append)
        window.append(RegisterManager::instance()['"']);
    else
        window.insert(RegisterManager::instance()['"']);
    window.clear_selections();
}

void do_select_regex(Window& window, int count)
{
    try
    {
        std::string ex = prompt("select: ");
        window.multi_select(std::bind(select_all_matches, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_split_regex(Window& window, int count)
{
    try
    {
        std::string ex = prompt("split: ");
        window.multi_select(std::bind(split_selection, _1, ex));
    }
    catch (prompt_aborted&) {}
}

std::unordered_map<char, std::function<void (Window& window, int count)>> keymap =
{
    { 'h', [](Window& window, int count) { window.move_cursor(DisplayCoord(0, -std::max(count,1))); } },
    { 'j', [](Window& window, int count) { window.move_cursor(DisplayCoord( std::max(count,1), 0)); } },
    { 'k', [](Window& window, int count) { window.move_cursor(DisplayCoord(-std::max(count,1), 0)); } },
    { 'l', [](Window& window, int count) { window.move_cursor(DisplayCoord(0,  std::max(count,1))); } },

    { 'H', [](Window& window, int count) { window.move_cursor(DisplayCoord(0, -std::max(count,1)), true); } },
    { 'J', [](Window& window, int count) { window.move_cursor(DisplayCoord( std::max(count,1), 0), true); } },
    { 'K', [](Window& window, int count) { window.move_cursor(DisplayCoord(-std::max(count,1), 0), true); } },
    { 'L', [](Window& window, int count) { window.move_cursor(DisplayCoord(0,  std::max(count,1)), true); } },

    { 't', [](Window& window, int count) { window.select(std::bind(select_to, _1, getch(), count, false)); } },
    { 'f', [](Window& window, int count) { window.select(std::bind(select_to, _1, getch(), count, true)); } },
    { 'T', [](Window& window, int count) { window.select(std::bind(select_to, _1, getch(), count, false), true); } },
    { 'F', [](Window& window, int count) { window.select(std::bind(select_to, _1, getch(), count, true), true); } },

    { 'd', do_erase },
    { 'c', do_change },
    { 'i', [](Window& window, int count) { do_insert(window, IncrementalInserter::Mode::Insert); } },
    { 'I', [](Window& window, int count) { do_insert(window, IncrementalInserter::Mode::InsertAtLineBegin); } },
    { 'a', [](Window& window, int count) { do_insert(window, IncrementalInserter::Mode::Append); } },
    { 'A', [](Window& window, int count) { do_insert(window, IncrementalInserter::Mode::AppendAtLineEnd); } },
    { 'o', [](Window& window, int count) { do_insert(window, IncrementalInserter::Mode::OpenLineBelow); } },
    { 'O', [](Window& window, int count) { do_insert(window, IncrementalInserter::Mode::OpenLineAbove); } },

    { 'g', do_go<false> },
    { 'G', do_go<true> },

    { 'y', do_yank },
    { 'p', do_paste<true> },
    { 'P', do_paste<false> },

    { 's', do_select_regex },

    { '%', [](Window& window, int count) { window.select([](const BufferIterator& cursor)
                                                         { return Selection(cursor.buffer().begin(), cursor.buffer().end()-1); }); } },

    { ':', [](Window& window, int count) { do_command(); } },
    { ' ', [](Window& window, int count) { window.clear_selections(); } },
    { 'w', [](Window& window, int count) { do { window.select(select_to_next_word); } while(--count > 0); } },
    { 'e', [](Window& window, int count) { do { window.select(select_to_next_word_end); } while(--count > 0); } },
    { 'b', [](Window& window, int count) { do { window.select(select_to_previous_word); } while(--count > 0); } },
    { 'W', [](Window& window, int count) { do { window.select(select_to_next_word, true); } while(--count > 0); } },
    { 'E', [](Window& window, int count) { do { window.select(select_to_next_word_end, true); } while(--count > 0); } },
    { 'B', [](Window& window, int count) { do { window.select(select_to_previous_word, true); } while(--count > 0); } },
    { 'x', [](Window& window, int count) { do { window.select(select_line, false); } while(--count > 0); } },
    { 'X', [](Window& window, int count) { do { window.select(select_line, true); } while(--count > 0); } },
    { 'm', [](Window& window, int count) { window.select(select_matching); } },
    { 'M', [](Window& window, int count) { window.select(select_matching, true); } },
    { '/', [](Window& window, int count) { do_search(window); } },
    { 'n', [](Window& window, int count) { do_search_next(window); } },
    { 'u', [](Window& window, int count) { do { if (not window.undo()) { print_status("nothing left to undo"); break; } } while(--count > 0); } },
    { 'U', [](Window& window, int count) { do { if (not window.redo()) { print_status("nothing left to redo"); break; } } while(--count > 0); } },
    { ',', [](Window& window, int count) { window.multi_select(select_whole_lines); } },
};

std::unordered_map<char, std::function<void (Window& window, int count)>> alt_keymap =
{
    { 't', [](Window& window, int count) { window.select(std::bind(select_to_reverse, _1, getch(), count, false)); } },
    { 'f', [](Window& window, int count) { window.select(std::bind(select_to_reverse, _1, getch(), count, true)); } },
    { 'T', [](Window& window, int count) { window.select(std::bind(select_to_reverse, _1, getch(), count, false), true); } },
    { 'F', [](Window& window, int count) { window.select(std::bind(select_to_reverse, _1, getch(), count, true), true); } },

    { 'w', [](Window& window, int count) { do { window.select(select_to_next_WORD); } while(--count > 0); } },
    { 'e', [](Window& window, int count) { do { window.select(select_to_next_WORD_end); } while(--count > 0); } },
    { 'b', [](Window& window, int count) { do { window.select(select_to_previous_WORD); } while(--count > 0); } },
    { 'W', [](Window& window, int count) { do { window.select(select_to_next_WORD, true); } while(--count > 0); } },
    { 'E', [](Window& window, int count) { do { window.select(select_to_next_WORD_end, true); } while(--count > 0); } },
    { 'B', [](Window& window, int count) { do { window.select(select_to_previous_WORD, true); } while(--count > 0); } },

    { 'l', [](Window& window, int count) { do { window.select(select_to_eol, false); } while(--count > 0); } },
    { 'L', [](Window& window, int count) { do { window.select(select_to_eol, true); } while(--count > 0); } },
    { 'h', [](Window& window, int count) { do { window.select(select_to_eol_reverse, false); } while(--count > 0); } },
    { 'H', [](Window& window, int count) { do { window.select(select_to_eol_reverse, true); } while(--count > 0); } },

    { 's', do_split_regex },
};

int main(int argc, char* argv[])
{
    init_ncurses();

    CommandManager  command_manager;
    BufferManager   buffer_manager;
    RegisterManager register_manager;
    FilterRegistry  filter_registry;

    command_manager.register_command(std::vector<std::string>{ "e", "edit" }, edit,
                                     PerArgumentCommandCompleter{ complete_filename });
    command_manager.register_command(std::vector<std::string>{ "q", "quit" }, quit<false>);
    command_manager.register_command(std::vector<std::string>{ "q!", "quit!" }, quit<true>);
    command_manager.register_command(std::vector<std::string>{ "w", "write" }, write_buffer,
                                     PerArgumentCommandCompleter{ complete_filename });
    command_manager.register_command(std::vector<std::string>{ "b", "buffer" }, show_buffer,
                                     PerArgumentCommandCompleter {
                                         std::bind(&BufferManager::complete_buffername, &buffer_manager, _1, _2)
                                      });
    command_manager.register_command(std::vector<std::string>{ "af", "addfilter" }, add_filter,
                                     PerArgumentCommandCompleter {
                                         std::bind(&FilterRegistry::complete_filter, &filter_registry, _1, _2)
                                     });
    command_manager.register_command(std::vector<std::string>{ "rf", "rmfilter" }, rm_filter,
                                     PerArgumentCommandCompleter {
                                         [&](const std::string& prefix, size_t cursor_pos)
                                         { return current_window->complete_filterid(prefix, cursor_pos); }
                                     });

    register_filters();

    try
    {
        auto buffer = (argc > 1) ? open_or_create(argv[1]) : new Buffer("*scratch*", Buffer::Type::Scratch);
        current_window = buffer->get_or_create_window();

        draw_window(*current_window);
        int count = 0;
        while(not quit_requested)
        {
            try
            {
                char c = getch();

                std::ostringstream oss;
                oss << "key " << int(c) << " (" << c << ")\n";
                write_debug(oss.str());

                if (isdigit(c))
                    count = count * 10 + c - '0';
                else
                {
                    bool is_alt = false;
                    if (c == 27)
                    {
                        timeout(0);
                        char new_c = getch();
                        timeout(-1);
                        if (new_c != ERR)
                        {
                            c = new_c;
                            is_alt = true;
                        }
                    }
                    auto& active_keymap = is_alt ? alt_keymap : keymap;

                    if (active_keymap.find(c) != active_keymap.end())
                    {
                        active_keymap[c](*current_window, count);
                        draw_window(*current_window);
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
