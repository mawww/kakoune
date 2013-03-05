#include "ncurses.hh"

#include "display_buffer.hh"
#include "register_manager.hh"

#include "utf8_iterator.hh"
#include "event_manager.hh"

#include <map>

#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>

namespace Kakoune
{

static void set_attribute(int attribute, bool on)
{
    if (on)
        attron(attribute);
    else
        attroff(attribute);
}

static int nc_color(Color color)
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
        return -1;
    }
}

static int get_color_pair(Color fg_color, Color bg_color)
{
    static std::map<std::pair<Color, Color>, int> colorpairs;
    static int next_pair = 1;

    std::pair<Color, Color> colorpair(fg_color, bg_color);

    auto it = colorpairs.find(colorpair);
    if (it != colorpairs.end())
        return it->second;
    else
    {
        init_pair(next_pair, nc_color(fg_color), nc_color(bg_color));
        colorpairs[colorpair] = next_pair;
        return next_pair++;
    }
}

static void set_color(Color fg_color, Color bg_color)
{
    static int current_pair = -1;

    if (current_pair != -1)
        attroff(COLOR_PAIR(current_pair));

    if (fg_color != Color::Default or bg_color != Color::Default)
    {
        current_pair = get_color_pair(fg_color, bg_color);
        attron(COLOR_PAIR(current_pair));
    }
}

void on_term_resize(int)
{
    ungetch(KEY_RESIZE);
    EventManager::instance().force_signal(0);
}

void on_sigint(int)
{
    ungetch(CTRL('c'));
    EventManager::instance().force_signal(0);
}

NCursesUI::NCursesUI()
    : m_stdin_watcher{0, [this](FDWatcher&){ if (m_input_callback) m_input_callback(); }}
{
    initscr();
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, false);
    keypad(stdscr, true);
    curs_set(0);
    start_color();
    use_default_colors();
    ESCDELAY=25;

    m_menu_fg = get_color_pair(Color::Blue, Color::Cyan);
    m_menu_bg = get_color_pair(Color::Cyan, Color::Blue);

    signal(SIGWINCH, on_term_resize);
    signal(SIGINT, on_sigint);

    update_dimensions();
}

NCursesUI::~NCursesUI()
{
    endwin();
    signal(SIGWINCH, SIG_DFL);
    signal(SIGINT, SIG_DFL);
}

void NCursesUI::redraw()
{
    redrawwin(stdscr);
    wnoutrefresh(stdscr);
    if (m_menu_win)
    {
        redrawwin(m_menu_win);
        wnoutrefresh(m_menu_win);
    }
    if (m_info_win)
    {
        redrawwin(m_info_win);
        wnoutrefresh(m_info_win);
    }
    doupdate();
}
using Utf8Policy = utf8::InvalidBytePolicy::Pass;
using Utf8Iterator = utf8::utf8_iterator<String::iterator, Utf8Policy>;
void addutf8str(WINDOW* win, Utf8Iterator begin, Utf8Iterator end)
{
    waddstr(win, std::string(begin.underlying_iterator(), end.underlying_iterator()).c_str());
}

static DisplayCoord window_size(WINDOW* win)
{
    DisplayCoord size;
    getmaxyx(win, (int&)size.line, (int&)size.column);
    return size;
}

static DisplayCoord window_pos(WINDOW* win)
{
    DisplayCoord pos;
    getbegyx(win, (int&)pos.line, (int&)pos.column);
    return pos;
}

void NCursesUI::update_dimensions()
{
    m_dimensions = window_size(stdscr);
    --m_dimensions.line;
}

void NCursesUI::draw(const DisplayBuffer& display_buffer,
                     const String& mode_line)
{
    LineCount line_index = 0;
    for (const DisplayLine& line : display_buffer.lines())
    {
        move((int)line_index, 0);
        clrtoeol();
        CharCount col_index = 0;
        for (const DisplayAtom& atom : line)
        {
            set_attribute(A_UNDERLINE, atom.attribute & Underline);
            set_attribute(A_REVERSE, atom.attribute & Reverse);
            set_attribute(A_BLINK, atom.attribute & Blink);
            set_attribute(A_BOLD, atom.attribute & Bold);

            set_color(atom.fg_color, atom.bg_color);

            String content = atom.content.content();
            if (content[content.length()-1] == '\n' and
                content.char_length() - 1 < m_dimensions.column - col_index)
            {
                addutf8str(stdscr, Utf8Iterator(content.begin()), Utf8Iterator(content.end())-1);
                addch(' ');
            }
            else
            {
                Utf8Iterator begin(content.begin()), end(content.end());
                if (end - begin > m_dimensions.column - col_index)
                    end = begin + (m_dimensions.column - col_index);
                addutf8str(stdscr, begin, end);
                col_index += end - begin;
            }
        }
        ++line_index;
    }

    set_attribute(A_UNDERLINE, 0);
    set_attribute(A_REVERSE, 0);
    set_attribute(A_BLINK, 0);
    set_attribute(A_BOLD, 0);
    set_color(Color::Blue, Color::Default);
    for (;line_index < m_dimensions.line; ++line_index)
    {
        move((int)line_index, 0);
        clrtoeol();
        addch('~');
    }

    set_color(Color::Cyan, Color::Default);
    draw_status();
    CharCount status_len = mode_line.char_length();
    // only draw mode_line if it does not overlap one status line
    if (m_dimensions.column - m_status_line.char_length() > status_len + 1)
    {
        move((int)m_dimensions.line, (int)(m_dimensions.column - status_len));
        addutf8str(stdscr, Utf8Iterator(mode_line.begin()),
                   Utf8Iterator(mode_line.end()));
    }
    redraw();
}

bool NCursesUI::is_key_available()
{
    timeout(0);
    const int c = getch();
    if (c != ERR)
        ungetch(c);
    timeout(-1);
    return c != ERR;
}

Key NCursesUI::get_key()
{
    const int c = getch();
    if (c > 0 and c < 27)
    {
        return {Key::Modifiers::Control, Codepoint(c) - 1 + 'a'};
    }
    else if (c == 27)
    {
        timeout(0);
        const Codepoint new_c = getch();
        timeout(-1);
        if (new_c != ERR)
            return {Key::Modifiers::Alt, new_c};
        else
            return Key::Escape;
    }
    else if (c == KEY_RESIZE)
    {
        int fd = open("/dev/tty", O_RDWR);
        winsize ws;
        if (fd != -1 and ioctl(fd, TIOCGWINSZ, (void*)&ws) == 0)
        {
            close(fd);
            resizeterm(ws.ws_row, ws.ws_col);
            update_dimensions();
        }
        return Key::Invalid;
    }
    else switch (c)
    {
    case KEY_BACKSPACE: case 127: return Key::Backspace;
    case KEY_UP: return Key::Up;
    case KEY_DOWN: return Key::Down;
    case KEY_LEFT: return Key::Left;
    case KEY_RIGHT: return Key::Right;
    case KEY_PPAGE: return Key::PageUp;
    case KEY_NPAGE: return Key::PageDown;
    case KEY_HOME: return Key::Home;
    case KEY_END: return Key::End;
    case KEY_BTAB: return Key::BackTab;
    }

    if (c >= 0 and c < 256)
    {
       ungetch(c);
       struct getch_iterator
       {
            int operator*() { return getch(); }
            getch_iterator& operator++() { return *this; }
            getch_iterator& operator++(int) { return *this; }
       };
       return utf8::codepoint(getch_iterator{});
    }
    return Key::Invalid;
}

void NCursesUI::draw_status()
{
    move((int)m_dimensions.line, 0);
    clrtoeol();
    if (m_status_cursor == -1)
       addutf8str(stdscr, m_status_line.begin(), m_status_line.end());
    else
    {
        auto cursor_it = utf8::advance(m_status_line.begin(), m_status_line.end(),
                                       (int)m_status_cursor);
        auto end = m_status_line.end();
        addutf8str(stdscr, m_status_line.begin(), cursor_it);
        set_attribute(A_REVERSE, 1);
        addch((cursor_it == end) ? ' ' : utf8::codepoint<Utf8Policy>(cursor_it));
        set_attribute(A_REVERSE, 0);
        if (cursor_it != end)
            addutf8str(stdscr, utf8::next(cursor_it), end);
    }
}

void NCursesUI::print_status(const String& status, CharCount cursor_pos)
{
    m_status_line   = status;
    m_status_cursor = cursor_pos;
    draw_status();
    redraw();
}

void NCursesUI::menu_show(const memoryview<String>& choices,
                          const DisplayCoord& anchor, MenuStyle style)
{
    assert(m_menu == nullptr);
    assert(m_menu_win == nullptr);
    assert(m_choices.empty());
    assert(m_items.empty());

    DisplayCoord maxsize = window_size(stdscr);
    maxsize.column -= anchor.column;

    m_choices.reserve(choices.size());
    CharCount longest = 0;
    for (auto& choice : choices)
    {
        m_choices.push_back(choice.substr(0_char, std::min((int)maxsize.column-1, 200)));
        m_items.emplace_back(new_item(m_choices.back().c_str(), ""));
        longest = std::max(longest, m_choices.back().char_length());
    }
    m_items.push_back(nullptr);
    longest += 1;

    int columns = (style == MenuStyle::Prompt) ? (int)(maxsize.column / longest) : 1;
    int lines = std::min(10, (int)ceilf((float)m_choices.size()/columns));

    DisplayCoord pos = { anchor.line+1, anchor.column };
    if (pos.line + lines >= maxsize.line)
        pos.line = anchor.line - lines;
    DisplayCoord size = { lines, columns == 1 ? longest : maxsize.column };

    m_menu = new_menu(&m_items[0]);
    m_menu_win = newwin((int)size.line, (int)size.column,
                        (int)pos.line,  (int)pos.column);
    set_menu_win(m_menu, m_menu_win);
    set_menu_format(m_menu, lines, columns);
    set_menu_mark(m_menu, nullptr);
    set_menu_fore(m_menu, COLOR_PAIR(m_menu_fg));
    set_menu_back(m_menu, COLOR_PAIR(m_menu_bg));
    post_menu(m_menu);
    redraw();
}

void NCursesUI::menu_select(int selected)
{
    // last item in m_items is the nullptr, hence the - 1
    if (selected >= 0 and selected < m_items.size() - 1)
    {
        set_menu_fore(m_menu, COLOR_PAIR(m_menu_fg));
        set_current_item(m_menu, m_items[selected]);
    }
    else
        set_menu_fore(m_menu, COLOR_PAIR(m_menu_bg));
    redraw();
}

void NCursesUI::menu_hide()
{
    if (not m_menu)
        return;
    unpost_menu(m_menu);
    free_menu(m_menu);
    for (auto item : m_items)
       if (item)
           free_item(item);
    m_menu = nullptr;
    delwin(m_menu_win);
    m_menu_win = nullptr;
    m_items.clear();
    m_choices.clear();
    redraw();
}

static DisplayCoord compute_needed_size(const String& str)
{
    DisplayCoord res{1,0};
    CharCount line_len = 0;
    for (Utf8Iterator begin{str.begin()}, end{str.end()}; begin != end; ++begin)
    {
        if (*begin == '\n')
        {
            // ignore last '\n', no need to show an empty line
            if (begin+1 == end)
                break;

            res.column = std::max(res.column, line_len+1);
            line_len = 0;
            ++res.line;
        }
        else
        {
            ++line_len;
            res.column = std::max(res.column, line_len);
        }
    }
    return res;
}

static DisplayCoord compute_pos(const DisplayCoord& anchor,
                                const DisplayCoord& size,
                                WINDOW* opt_window_to_avoid = nullptr)
{
    DisplayCoord scrsize = window_size(stdscr);
    DisplayCoord pos = { anchor.line+1, anchor.column };
    if (pos.line + size.line >= scrsize.line)
        pos.line = anchor.line - size.line;

    if (opt_window_to_avoid)
    {
        DisplayCoord winbeg = window_pos(opt_window_to_avoid);
        DisplayCoord winend = winbeg + window_size(opt_window_to_avoid);

        DisplayCoord end = pos + size;

        // check intersection
        if (not (end.line < winbeg.line or end.column < winbeg.column or
                 pos.line > winend.line or pos.column > winend.column))
        {
            pos.line = std::min(winbeg.line, anchor.line) - size.line;
            // if above does not work, try below
            if (pos.line < 0)
                pos.line = std::max(winend.line, anchor.line);
        }
    }

    return pos;
}

void NCursesUI::info_show(const String& content, const DisplayCoord& anchor, MenuStyle style)
{
    assert(m_info_win == nullptr);

    DisplayCoord size = compute_needed_size(content);
    if (style == MenuStyle::Prompt)
        size.column = window_size(stdscr).column - anchor.column;

    DisplayCoord pos = compute_pos(anchor, size, m_menu_win);

    m_info_win = newwin((int)size.line, (int)size.column,
                        (int)pos.line,  (int)pos.column);

    wbkgd(m_info_win, COLOR_PAIR(get_color_pair(Color::Black, Color::Yellow)));
    wmove(m_info_win, 0, 0);
    addutf8str(m_info_win, Utf8Iterator(content.begin()),
               Utf8Iterator(content.end()));
    redraw();
}

void NCursesUI::info_hide()
{
    if (not m_info_win)
        return;
    delwin(m_info_win);
    m_info_win = nullptr;
    redraw();
}

DisplayCoord NCursesUI::dimensions()
{
    return m_dimensions;
}

void NCursesUI::set_input_callback(InputCallback callback)
{
    m_input_callback = std::move(callback);
}

}
