#include "ncurses.hh"

#include "display_buffer.hh"
#include "event_manager.hh"
#include "register_manager.hh"
#include "utf8_iterator.hh"

#include <map>

#define NCURSES_OPAQUE 0
#define NCURSES_INTERNALS

#include <ncursesw/ncurses.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>

namespace Kakoune
{

struct NCursesWin : WINDOW {};

static void set_attribute(int attribute, bool on)
{
    if (on)
        attron(attribute);
    else
        attroff(attribute);
}

static bool operator<(const Color& lhs, const Color& rhs)
{
    if (lhs.color == rhs.color and lhs.color == Colors::RGB)
        return lhs.r == rhs.r ? (lhs.g == rhs.g ? lhs.b < rhs.b
                                                : lhs.g < rhs.g)
                              : lhs.r < rhs.r;
    return lhs.color < rhs.color;
}

static int nc_color(const Color& color)
{
    static std::map<Color, int> colors = {
        { Colors::Default, -1 },
        { Colors::Black,   COLOR_BLACK },
        { Colors::Red,     COLOR_RED },
        { Colors::Green,   COLOR_GREEN },
        { Colors::Yellow,  COLOR_YELLOW },
        { Colors::Blue,    COLOR_BLUE },
        { Colors::Magenta, COLOR_MAGENTA },
        { Colors::Cyan,    COLOR_CYAN },
        { Colors::White,   COLOR_WHITE },
    };
    static int next_color = 8;

    auto it = colors.find(color);
    if (it != colors.end())
        return it->second;
    else if (can_change_color() and COLORS > 8)
    {
        kak_assert(color.color == Colors::RGB);
        if (next_color > COLORS)
            next_color = 8;
        init_color(next_color,
                   color.r * 1000 / 255,
                   color.g * 1000 / 255,
                   color.b * 1000 / 255);
        colors[color] = next_color;
        return next_color++;
    }
    else
    {
        kak_assert(color.color == Colors::RGB);
        // project to closest color.
        struct BuiltinColor { int id; unsigned char r, g, b; };
        static constexpr BuiltinColor builtins[] = {
            { COLOR_BLACK,   0,   0,   0 },
            { COLOR_RED,     255, 0,   0 },
            { COLOR_GREEN,   0,   255, 0 },
            { COLOR_YELLOW,  255, 255, 0 },
            { COLOR_BLUE,    0,   0,   255 },
            { COLOR_MAGENTA, 255, 0,   255 },
            { COLOR_CYAN,    0,   255, 255 },
            { COLOR_WHITE,   255, 255, 255 }
        };
        auto sq = [](int x) { return x * x; };
        int lowestDist = INT_MAX;
        int closestCol = -1;
        for (auto& col : builtins)
        {
            int dist = sq(color.r - col.r) + sq(color.g - col.g) + sq(color.b - col.b);
            if (dist < lowestDist)
            {
                lowestDist = dist;
                closestCol = col.id;
            }
        }
        return closestCol;
    }
}

static int get_color_pair(const ColorPair& colors)
{
    static std::map<ColorPair, int> colorpairs;
    static int next_pair = 1;

    auto it = colorpairs.find(colors);
    if (it != colorpairs.end())
        return it->second;
    else
    {
        init_pair(next_pair, nc_color(colors.first), nc_color(colors.second));
        colorpairs[colors] = next_pair;
        return next_pair++;
    }
}

static void set_color(WINDOW* window, const ColorPair colors)
{
    static int current_pair = -1;

    if (current_pair != -1)
        wattroff(window, COLOR_PAIR(current_pair));

    if (colors.first != Colors::Default or colors.second != Colors::Default)
    {
        current_pair = get_color_pair(colors);
        wattron(window, COLOR_PAIR(current_pair));
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
    set_escdelay(25);

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
using Utf8Iterator = utf8::utf8_iterator<String::const_iterator, Utf8Policy>;
void addutf8str(WINDOW* win, Utf8Iterator begin, Utf8Iterator end)
{
    waddstr(win, std::string(begin.base(), end.base()).c_str());
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

void NCursesUI::draw_line(const DisplayLine& line, CharCount col_index) const
{
    for (const DisplayAtom& atom : line)
    {
        set_attribute(A_UNDERLINE, atom.attribute & Underline);
        set_attribute(A_REVERSE, atom.attribute & Reverse);
        set_attribute(A_BLINK, atom.attribute & Blink);
        set_attribute(A_BOLD, atom.attribute & Bold);

        set_color(stdscr, atom.colors);

        String content = atom.content();
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
}

void NCursesUI::draw(const DisplayBuffer& display_buffer,
                     const DisplayLine& mode_line)
{
    LineCount line_index = 0;
    for (const DisplayLine& line : display_buffer.lines())
    {
        wmove(stdscr, (int)line_index, 0);
        wclrtoeol(stdscr);
            draw_line(line, 0);
        ++line_index;
    }

    set_attribute(A_UNDERLINE, 0);
    set_attribute(A_REVERSE, 0);
    set_attribute(A_BLINK, 0);
    set_attribute(A_BOLD, 0);
    set_color(stdscr, { Colors::Blue, Colors::Default });
    for (;line_index < m_dimensions.line; ++line_index)
    {
        move((int)line_index, 0);
        clrtoeol();
        addch('~');
    }

    move((int)m_dimensions.line, 0);
    clrtoeol();
    draw_line(m_status_line, 0);
    CharCount status_len = mode_line.length();
    // only draw mode_line if it does not overlap one status line
    if (m_dimensions.column - m_status_line.length() > status_len + 1)
    {
        CharCount col = m_dimensions.column - status_len;
        move((int)m_dimensions.line, (int)col);
        draw_line(mode_line, col);
    }

    const char* tsl = tigetstr((char*)"tsl");
    const char* fsl = tigetstr((char*)"fsl");
    if (tsl != 0 and (ptrdiff_t)tsl != -1 and fsl != 0 and (ptrdiff_t)fsl != -1)
    {
        String title;
        for (auto& atom : mode_line)
            title += atom.content();
        title += " - Kakoune";
        printf("%s%s%s", tsl, title.c_str(), fsl);
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
        if (c == CTRL('l'))
           redrawwin(stdscr);
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

void NCursesUI::print_status(const DisplayLine& status)
{
    m_status_line   = status;
    move((int)m_dimensions.line, 0);
    clrtoeol();
    draw_line(status, 0);
    redraw();
}

void NCursesUI::draw_menu()
{
    // menu show may have not created the window if it did not fit.
    // so be tolerant.
    if (not m_menu_win)
        return;

    auto menu_fg = get_color_pair(m_menu_fg);
    auto menu_bg = get_color_pair(m_menu_bg);

    auto scroll_fg = get_color_pair({ Colors::White, Colors::White });
    auto scroll_bg = get_color_pair(m_menu_bg);

    wattron(m_menu_win, COLOR_PAIR(menu_bg));
    wbkgdset(m_menu_win, COLOR_PAIR(menu_bg));
    DisplayCoord menu_size = window_size(m_menu_win);
    CharCount column_width = (menu_size.column - 1) / m_menu_columns;
    LineCount mark_line = (menu_size.line * m_selected_choice) / (int)m_choices.size();
    for (auto line = 0_line; line < menu_size.line; ++line)
    {
        wmove(m_menu_win, (int)line, 0);
        for (int col = 0; col < m_menu_columns; ++col)
        {
            int choice_idx = (int)(m_menu_top_line + line) * m_menu_columns + col;
            if (choice_idx >= m_choices.size())
                break;
            if (choice_idx == m_selected_choice)
                wattron(m_menu_win, COLOR_PAIR(menu_fg));

            auto& choice = m_choices[choice_idx];
            auto begin = choice.cbegin();
            auto end = utf8::advance(begin, choice.cend(), column_width);
            addutf8str(m_menu_win, begin, end);
            for (auto pad = column_width - utf8::distance(begin, end); pad > 0; --pad)
                waddch(m_menu_win, ' ');
            wattron(m_menu_win, COLOR_PAIR(menu_bg));
        }
        wclrtoeol(m_menu_win);
        wmove(m_menu_win, (int)line, (int)menu_size.column - 1);
        wattron(m_menu_win, COLOR_PAIR(line == mark_line ? scroll_fg : scroll_bg));
        waddch(m_menu_win, ' ');
        wattron(m_menu_win, COLOR_PAIR(menu_bg));
    }
    redraw();
}

void NCursesUI::menu_show(const memoryview<String>& choices,
                          DisplayCoord anchor, ColorPair fg, ColorPair bg,
                          MenuStyle style)
{
    kak_assert(m_menu_win == nullptr);
    kak_assert(m_choices.empty());

    m_menu_fg = fg;
    m_menu_bg = bg;

    DisplayCoord maxsize = window_size(stdscr);
    maxsize.column -= anchor.column;
    if (maxsize.column <= 2)
        return;

    m_choices.reserve(choices.size());
    CharCount longest = 0;
    const CharCount maxlen = std::min((int)maxsize.column-2, 200);
    for (auto& choice : choices)
    {
        m_choices.push_back(choice.substr(0_char, maxlen));
        longest = std::max(longest, m_choices.back().char_length());
    }
    longest += 1;

    m_menu_columns = (style == MenuStyle::Prompt) ? (int)((maxsize.column -1) / longest) : 1;
    int lines = std::min(10, (int)ceilf((float)m_choices.size()/m_menu_columns));

    DisplayCoord pos = { anchor.line+1, anchor.column };
    if (pos.line + lines >= maxsize.line)
        pos.line = anchor.line - lines;
    DisplayCoord size = { lines, style == MenuStyle::Prompt ? maxsize.column : longest };

    m_selected_choice = 0;
    m_menu_top_line = 0;
    m_menu_win = (NCursesWin*)newwin((int)size.line, (int)size.column,
                                     (int)pos.line,  (int)pos.column);
    draw_menu();
}

void NCursesUI::menu_select(int selected)
{
    if (selected < 0 or selected >= m_choices.size())
    {
        m_selected_choice = -1;
        m_menu_top_line = 0;
    }
    else
    {
        m_selected_choice = selected;
        LineCount selected_line = m_selected_choice / m_menu_columns;
        DisplayCoord menu_size = window_size(m_menu_win);
        if (selected_line < m_menu_top_line)
            m_menu_top_line = selected_line;
        if (selected_line >= m_menu_top_line + menu_size.line)
            m_menu_top_line = selected_line;
    }

    draw_menu();
}

void NCursesUI::menu_hide()
{
    if (not m_menu_win)
        return;
    m_choices.clear();
    wredrawln(stdscr, (int)window_pos(m_menu_win).line, (int)window_size(m_menu_win).line);
    delwin(m_menu_win);
    m_menu_win = nullptr;
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

            res.column = std::max(res.column, line_len);
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
        pos.line = std::max(0_line, anchor.line - size.line);
    if (pos.column + size.column >= scrsize.column)
        pos.column = std::max(0_char, anchor.column - size.column+1);

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

void NCursesUI::info_show(const String& content, DisplayCoord anchor,
                          ColorPair colors, MenuStyle style)
{
    kak_assert(m_info_win == nullptr);

    DisplayCoord size = compute_needed_size(content);
    if (style == MenuStyle::Prompt)
        size.column = window_size(stdscr).column - anchor.column;

    DisplayCoord pos = compute_pos(anchor, size, m_menu_win);

    m_info_win = (NCursesWin*)newwin((int)size.line, (int)size.column,
                                     (int)pos.line,  (int)pos.column);

    wbkgd(m_info_win, COLOR_PAIR(get_color_pair(colors)));
    int line = 0;
    auto it = content.begin(), end = content.end();
    while (true)
    {
        wmove(m_info_win, line++, 0);
        auto eol = std::find_if(it, end, [](char c) { return c == '\n'; });
        addutf8str(m_info_win, Utf8Iterator(it), Utf8Iterator(eol));
        if (eol == end)
           break;
        it = eol + 1;
    }
    redraw();
}

void NCursesUI::info_hide()
{
    if (not m_info_win)
        return;
    wredrawln(stdscr, (int)window_pos(m_info_win).line, (int)window_size(m_info_win).line);
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

void NCursesUI::abort()
{
    endwin();
}

}
