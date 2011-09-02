#include <ncurses.h>
#include "window.hh"
#include "buffer.hh"
#include "file.hh"
#include "regex_selector.hh"

#include <unordered_map>
#include <cassert>

using namespace Kakoune;

void draw_window(Window& window)
{
    window.update_display_buffer();

    int max_x,max_y;
    getmaxyx(stdscr, max_y, max_x);
    max_y -= 1;

    LineAndColumn position;
    for (const DisplayAtom& atom : window.display_buffer())
    {
        const std::string& content = atom.content;

        if (atom.attribute & UNDERLINE)
            attron(A_UNDERLINE);
        else
            attroff(A_UNDERLINE);

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
    while (++position.line < max_y)
    {
        move(position.line, 0);
        clrtoeol();
        addch('~');
    }

    const LineAndColumn& cursor_position = window.cursor_position();
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
}

void deinit_ncurses()
{
    endwin();
}

struct prompt_aborted {};

std::string prompt(const std::string& text)
{
    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);
    move(max_y-1, 0);
    addstr(text.c_str());
    clrtoeol();

    std::string result;
    while(true)
    {
        char c = getch();
        switch (c)
        {
        case '\r':
            return result;
        case 7:
            if (not result.empty())
            {
                move(max_y - 1, text.length() + result.length() - 1);
                addch(' ');
                result.resize(result.length() - 1);
                move(max_y - 1, text.length() + result.length());
                refresh();
            }
            break;
        case 27:
            throw prompt_aborted();
        default:
            result += c;
            addch(c);
            refresh();
        }
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

void do_insert(Window& window)
{
    print_status("-- INSERT --");
    std::string inserted;
    LineAndColumn pos = window.cursor_position();
    move(pos.line, pos.column);
    refresh();
    while(true)
    {
        char c = getch();
        if (c == 27)
            break;

        window.insert(std::string() + c);
        draw_window(window);
    }
    print_status("");
}

std::shared_ptr<Window> current_window;

void edit(const std::string& filename)
{
    try
    {
        std::shared_ptr<Buffer> buffer(create_buffer_from_file(filename));
        if (buffer)
            current_window = std::make_shared<Window>(buffer);
    }
    catch (file_not_found& what)
    {
        current_window = std::make_shared<Window>(std::make_shared<Buffer>(filename));
    }
    catch (open_file_error& what)
    {
        print_status("error opening '" + filename + "' (" + what.what() + ")");
    }
}

void write_buffer(const std::string& filename)
{
    try
    {
        Buffer& buffer = *current_window->buffer();
        write_buffer_to_file(buffer,
                             filename.empty() ? buffer.name() : filename);
    }
    catch(open_file_error& what)
    {
        print_status("error opening " + filename + "(" + what.what() + ")");
    }
    catch(write_file_error& what)
    {
        print_status("error writing " + filename + "(" + what.what() + ")");
    }
}

bool quit_requested = false;

void quit(const std::string&)
{
    quit_requested = true;
}

std::unordered_map<std::string, std::function<void (const std::string& param)>> cmdmap =
{
    { "e", edit },
    { "edit", edit },
    { "q", quit },
    { "quit", quit },
    { "w", write_buffer },
    { "write", write_buffer },
};

void do_command()
{
    try
    {
        std::string cmd = prompt(":");

        size_t cmd_end = cmd.find_first_of(' ');
        std::string cmd_name = cmd.substr(0, cmd_end);
        size_t param_start = cmd.find_first_not_of(' ', cmd_end);
        std::string param;
        if (param_start != std::string::npos)
            param = cmd.substr(param_start, cmd.length() - param_start);

        if (cmdmap.find(cmd_name) != cmdmap.end())
            cmdmap[cmd_name](param);
        else
            print_status(cmd_name + ": no such command");
    }
    catch (prompt_aborted&) {}
}

bool is_blank(char c)
{
    return c == ' ' or c == '\t' or c == '\n';
}

Selection select_to_next_word(const BufferIterator& cursor)
{
    BufferIterator end = cursor;
    while (not end.is_end() and not is_blank(*end))
        ++end;

    while (not end.is_end() and is_blank(*end))
        ++end;

    return Selection(cursor, end);
}

Selection select_to_next_word_end(const BufferIterator& cursor)
{
    BufferIterator end = cursor;
    while (not end.is_end() and is_blank(*end))
        ++end;

    while (not end.is_end() and not is_blank(*end))
        ++end;

    return Selection(cursor, end);
}

Selection select_line(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    while (not begin.is_begin() and *(begin -1) != '\n')
        --begin;

    BufferIterator end = cursor;
    while (not end.is_end() and *end != '\n')
        ++end;
    return Selection(begin, end + 1);
}

void do_search(Window& window)
{
    try
    {
        std::string ex = prompt("/");
        window.select(false, RegexSelector(ex));
    }
    catch (boost::regex_error&) {}
    catch (prompt_aborted&) {}
}

std::unordered_map<char, std::function<void (Window& window, int count)>> keymap =
{
    { 'h', [](Window& window, int count) { if (count == 0) count = 1; window.move_cursor(LineAndColumn(0, -count)); window.empty_selections(); } },
    { 'j', [](Window& window, int count) { if (count == 0) count = 1; window.move_cursor(LineAndColumn(count,  0)); window.empty_selections(); } },
    { 'k', [](Window& window, int count) { if (count == 0) count = 1; window.move_cursor(LineAndColumn(-count, 0)); window.empty_selections(); } },
    { 'l', [](Window& window, int count) { if (count == 0) count = 1; window.move_cursor(LineAndColumn(0,  count)); window.empty_selections(); } },
    { 'd', [](Window& window, int count) { window.erase(); window.empty_selections(); } },
    { 'c', [](Window& window, int count) { window.erase(); do_insert(window); } },
    { 'i', [](Window& window, int count) { do_insert(window); } },
    { ':', [](Window& window, int count) { do_command(); } },
    { ' ', [](Window& window, int count) { window.empty_selections(); } },
    { 'w', [](Window& window, int count) { do { window.select(false, select_to_next_word); } while(--count > 0); } },
    { 'W', [](Window& window, int count) { do { window.select(true, select_to_next_word); } while(--count > 0); } },
    { 'e', [](Window& window, int count) { do { window.select(false, select_to_next_word_end); } while(--count > 0); } },
    { 'E', [](Window& window, int count) { do { window.select(true, select_to_next_word_end); } while(--count > 0); } },
    { '.', [](Window& window, int count) { do { window.select(false, select_line); } while(--count > 0); } },
    { '/', [](Window& window, int count) { do_search(window); } },
};

int main()
{
    init_ncurses();

    try
    {
        auto buffer = std::make_shared<Buffer>("<scratch>");
        current_window = std::make_shared<Window>(buffer);

        draw_window(*current_window);
        int count = 0;
        while(not quit_requested)
        {
            char c = getch();

            if (isdigit(c))
                count = count * 10 + c - '0';
            else
            {
                if (keymap.find(c) != keymap.end())
                {
                    keymap[c](*current_window, count);
                    draw_window(*current_window);
                }
                count = 0;
            }
        }
        deinit_ncurses();
    }
    catch (...)
    {
        deinit_ncurses();
        throw;
    }
    return 0;
}
