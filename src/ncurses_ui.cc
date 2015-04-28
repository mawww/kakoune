#include "ncurses_ui.hh"

#include "containers.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "keys.hh"
#include "register_manager.hh"
#include "utf8_iterator.hh"

#include <map>
#include <algorithm>

#define NCURSES_OPAQUE 0
#define NCURSES_INTERNALS

#include <ncurses.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace Kakoune
{

using std::min;
using std::max;

struct NCursesWin : WINDOW {};

static constexpr StringView assistant_cat[] =
    { R"(  ___            )",
      R"( (__ \           )",
      R"(   / /          ╭)",
      R"(  .' '·.        │)",
      R"( '      ”       │)",
      R"( ╰       /\_/|  │)",
      R"(  | .         \ │)",
      R"(  ╰_J`    | | | ╯)",
      R"(      ' \__- _/  )",
      R"(      \_\   \_\  )",
      R"(                 )"};

static constexpr StringView assistant_clippy[] =
    { " ╭──╮   ",
      " │  │   ",
      " @  @  ╭",
      " ││ ││ │",
      " ││ ││ ╯",
      " │╰─╯│  ",
      " ╰───╯  ",
      "        " };

static void set_attribute(WINDOW* window, int attribute, bool on)
{
    if (on)
        wattron(window, attribute);
    else
        wattroff(window, attribute);
}

static bool operator<(Color lhs, Color rhs)
{
    if (lhs.color == rhs.color and lhs.color == Color::RGB)
        return lhs.r == rhs.r ? (lhs.g == rhs.g ? lhs.b < rhs.b
                                                : lhs.g < rhs.g)
                              : lhs.r < rhs.r;
    return lhs.color < rhs.color;
}

template<typename T> T sq(T x) { return x * x; }

static int nc_color(Color color)
{
    static std::map<Color, int> colors = {
        { Color::Default, -1 },
        { Color::Black,   COLOR_BLACK },
        { Color::Red,     COLOR_RED },
        { Color::Green,   COLOR_GREEN },
        { Color::Yellow,  COLOR_YELLOW },
        { Color::Blue,    COLOR_BLUE },
        { Color::Magenta, COLOR_MAGENTA },
        { Color::Cyan,    COLOR_CYAN },
        { Color::White,   COLOR_WHITE },
    };
    static int next_color = 8;

    auto it = colors.find(color);
    if (it != colors.end())
        return it->second;
    else if (can_change_color() and COLORS > 8)
    {
        kak_assert(color.color == Color::RGB);
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
        kak_assert(color.color == Color::RGB);
        static const struct { unsigned char r, g, b; } builtin_colors[] = {
            {0x00,0x00,0x00}, {0x80,0x00,0x00}, {0x00,0x80,0x00}, {0x80,0x80,0x00},
            {0x00,0x00,0x80}, {0x80,0x00,0x80}, {0x00,0x80,0x80}, {0xc0,0xc0,0xc0},
            {0x80,0x80,0x80}, {0xff,0x00,0x00}, {0x00,0xff,0x00}, {0xff,0xff,0x00},
            {0x00,0x00,0xff}, {0xff,0x00,0xff}, {0x00,0xff,0xff}, {0xff,0xff,0xff},
            {0x00,0x00,0x00}, {0x00,0x00,0x5f}, {0x00,0x00,0x87}, {0x00,0x00,0xaf},
            {0x00,0x00,0xd7}, {0x00,0x00,0xff}, {0x00,0x5f,0x00}, {0x00,0x5f,0x5f},
            {0x00,0x5f,0x87}, {0x00,0x5f,0xaf}, {0x00,0x5f,0xd7}, {0x00,0x5f,0xff},
            {0x00,0x87,0x00}, {0x00,0x87,0x5f}, {0x00,0x87,0x87}, {0x00,0x87,0xaf},
            {0x00,0x87,0xd7}, {0x00,0x87,0xff}, {0x00,0xaf,0x00}, {0x00,0xaf,0x5f},
            {0x00,0xaf,0x87}, {0x00,0xaf,0xaf}, {0x00,0xaf,0xd7}, {0x00,0xaf,0xff},
            {0x00,0xd7,0x00}, {0x00,0xd7,0x5f}, {0x00,0xd7,0x87}, {0x00,0xd7,0xaf},
            {0x00,0xd7,0xd7}, {0x00,0xd7,0xff}, {0x00,0xff,0x00}, {0x00,0xff,0x5f},
            {0x00,0xff,0x87}, {0x00,0xff,0xaf}, {0x00,0xff,0xd7}, {0x00,0xff,0xff},
            {0x5f,0x00,0x00}, {0x5f,0x00,0x5f}, {0x5f,0x00,0x87}, {0x5f,0x00,0xaf},
            {0x5f,0x00,0xd7}, {0x5f,0x00,0xff}, {0x5f,0x5f,0x00}, {0x5f,0x5f,0x5f},
            {0x5f,0x5f,0x87}, {0x5f,0x5f,0xaf}, {0x5f,0x5f,0xd7}, {0x5f,0x5f,0xff},
            {0x5f,0x87,0x00}, {0x5f,0x87,0x5f}, {0x5f,0x87,0x87}, {0x5f,0x87,0xaf},
            {0x5f,0x87,0xd7}, {0x5f,0x87,0xff}, {0x5f,0xaf,0x00}, {0x5f,0xaf,0x5f},
            {0x5f,0xaf,0x87}, {0x5f,0xaf,0xaf}, {0x5f,0xaf,0xd7}, {0x5f,0xaf,0xff},
            {0x5f,0xd7,0x00}, {0x5f,0xd7,0x5f}, {0x5f,0xd7,0x87}, {0x5f,0xd7,0xaf},
            {0x5f,0xd7,0xd7}, {0x5f,0xd7,0xff}, {0x5f,0xff,0x00}, {0x5f,0xff,0x5f},
            {0x5f,0xff,0x87}, {0x5f,0xff,0xaf}, {0x5f,0xff,0xd7}, {0x5f,0xff,0xff},
            {0x87,0x00,0x00}, {0x87,0x00,0x5f}, {0x87,0x00,0x87}, {0x87,0x00,0xaf},
            {0x87,0x00,0xd7}, {0x87,0x00,0xff}, {0x87,0x5f,0x00}, {0x87,0x5f,0x5f},
            {0x87,0x5f,0x87}, {0x87,0x5f,0xaf}, {0x87,0x5f,0xd7}, {0x87,0x5f,0xff},
            {0x87,0x87,0x00}, {0x87,0x87,0x5f}, {0x87,0x87,0x87}, {0x87,0x87,0xaf},
            {0x87,0x87,0xd7}, {0x87,0x87,0xff}, {0x87,0xaf,0x00}, {0x87,0xaf,0x5f},
            {0x87,0xaf,0x87}, {0x87,0xaf,0xaf}, {0x87,0xaf,0xd7}, {0x87,0xaf,0xff},
            {0x87,0xd7,0x00}, {0x87,0xd7,0x5f}, {0x87,0xd7,0x87}, {0x87,0xd7,0xaf},
            {0x87,0xd7,0xd7}, {0x87,0xd7,0xff}, {0x87,0xff,0x00}, {0x87,0xff,0x5f},
            {0x87,0xff,0x87}, {0x87,0xff,0xaf}, {0x87,0xff,0xd7}, {0x87,0xff,0xff},
            {0xaf,0x00,0x00}, {0xaf,0x00,0x5f}, {0xaf,0x00,0x87}, {0xaf,0x00,0xaf},
            {0xaf,0x00,0xd7}, {0xaf,0x00,0xff}, {0xaf,0x5f,0x00}, {0xaf,0x5f,0x5f},
            {0xaf,0x5f,0x87}, {0xaf,0x5f,0xaf}, {0xaf,0x5f,0xd7}, {0xaf,0x5f,0xff},
            {0xaf,0x87,0x00}, {0xaf,0x87,0x5f}, {0xaf,0x87,0x87}, {0xaf,0x87,0xaf},
            {0xaf,0x87,0xd7}, {0xaf,0x87,0xff}, {0xaf,0xaf,0x00}, {0xaf,0xaf,0x5f},
            {0xaf,0xaf,0x87}, {0xaf,0xaf,0xaf}, {0xaf,0xaf,0xd7}, {0xaf,0xaf,0xff},
            {0xaf,0xd7,0x00}, {0xaf,0xd7,0x5f}, {0xaf,0xd7,0x87}, {0xaf,0xd7,0xaf},
            {0xaf,0xd7,0xd7}, {0xaf,0xd7,0xff}, {0xaf,0xff,0x00}, {0xaf,0xff,0x5f},
            {0xaf,0xff,0x87}, {0xaf,0xff,0xaf}, {0xaf,0xff,0xd7}, {0xaf,0xff,0xff},
            {0xd7,0x00,0x00}, {0xd7,0x00,0x5f}, {0xd7,0x00,0x87}, {0xd7,0x00,0xaf},
            {0xd7,0x00,0xd7}, {0xd7,0x00,0xff}, {0xd7,0x5f,0x00}, {0xd7,0x5f,0x5f},
            {0xd7,0x5f,0x87}, {0xd7,0x5f,0xaf}, {0xd7,0x5f,0xd7}, {0xd7,0x5f,0xff},
            {0xd7,0x87,0x00}, {0xd7,0x87,0x5f}, {0xd7,0x87,0x87}, {0xd7,0x87,0xaf},
            {0xd7,0x87,0xd7}, {0xd7,0x87,0xff}, {0xd7,0xaf,0x00}, {0xd7,0xaf,0x5f},
            {0xd7,0xaf,0x87}, {0xd7,0xaf,0xaf}, {0xd7,0xaf,0xd7}, {0xd7,0xaf,0xff},
            {0xd7,0xd7,0x00}, {0xd7,0xd7,0x5f}, {0xd7,0xd7,0x87}, {0xd7,0xd7,0xaf},
            {0xd7,0xd7,0xd7}, {0xd7,0xd7,0xff}, {0xd7,0xff,0x00}, {0xd7,0xff,0x5f},
            {0xd7,0xff,0x87}, {0xd7,0xff,0xaf}, {0xd7,0xff,0xd7}, {0xd7,0xff,0xff},
            {0xff,0x00,0x00}, {0xff,0x00,0x5f}, {0xff,0x00,0x87}, {0xff,0x00,0xaf},
            {0xff,0x00,0xd7}, {0xff,0x00,0xff}, {0xff,0x5f,0x00}, {0xff,0x5f,0x5f},
            {0xff,0x5f,0x87}, {0xff,0x5f,0xaf}, {0xff,0x5f,0xd7}, {0xff,0x5f,0xff},
            {0xff,0x87,0x00}, {0xff,0x87,0x5f}, {0xff,0x87,0x87}, {0xff,0x87,0xaf},
            {0xff,0x87,0xd7}, {0xff,0x87,0xff}, {0xff,0xaf,0x00}, {0xff,0xaf,0x5f},
            {0xff,0xaf,0x87}, {0xff,0xaf,0xaf}, {0xff,0xaf,0xd7}, {0xff,0xaf,0xff},
            {0xff,0xd7,0x00}, {0xff,0xd7,0x5f}, {0xff,0xd7,0x87}, {0xff,0xd7,0xaf},
            {0xff,0xd7,0xd7}, {0xff,0xd7,0xff}, {0xff,0xff,0x00}, {0xff,0xff,0x5f},
            {0xff,0xff,0x87}, {0xff,0xff,0xaf}, {0xff,0xff,0xd7}, {0xff,0xff,0xff},
            {0x08,0x08,0x08}, {0x12,0x12,0x12}, {0x1c,0x1c,0x1c}, {0x26,0x26,0x26},
            {0x30,0x30,0x30}, {0x3a,0x3a,0x3a}, {0x44,0x44,0x44}, {0x4e,0x4e,0x4e},
            {0x58,0x58,0x58}, {0x60,0x60,0x60}, {0x66,0x66,0x66}, {0x76,0x76,0x76},
            {0x80,0x80,0x80}, {0x8a,0x8a,0x8a}, {0x94,0x94,0x94}, {0x9e,0x9e,0x9e},
            {0xa8,0xa8,0xa8}, {0xb2,0xb2,0xb2}, {0xbc,0xbc,0xbc}, {0xc6,0xc6,0xc6},
            {0xd0,0xd0,0xd0}, {0xda,0xda,0xda}, {0xe4,0xe4,0xe4}, {0xee,0xee,0xee},
        };
        int lowestDist = INT_MAX;
        int closestCol = -1;
        for (int i = 0; i < std::min(256, COLORS); ++i)
        {
            auto& col = builtin_colors[i];
            int dist = sq(color.r - col.r)
                     + sq(color.g - col.g)
                     + sq(color.b - col.b);
            if (dist < lowestDist)
            {
                lowestDist = dist;
                closestCol = i;
            }
        }
        return closestCol;
    }
}

static int get_color_pair(const Face& face)
{
    using ColorPair = std::pair<Color, Color>;
    static UnorderedMap<ColorPair, int> colorpairs;
    static int next_pair = 1;

    ColorPair colors{face.fg, face.bg};
    auto it = colorpairs.find(colors);
    if (it != colorpairs.end())
        return it->second;
    else
    {
        init_pair(next_pair, nc_color(face.fg), nc_color(face.bg));
        colorpairs[colors] = next_pair;
        return next_pair++;
    }
}

static void set_face(WINDOW* window, Face face)
{
    static int current_pair = -1;

    if (current_pair != -1)
        wattroff(window, COLOR_PAIR(current_pair));

    if (face.fg != Color::Default or face.bg != Color::Default)
    {
        current_pair = get_color_pair(face);
        wattron(window, COLOR_PAIR(current_pair));
    }

    set_attribute(window, A_UNDERLINE, face.attributes & Attribute::Underline);
    set_attribute(window, A_REVERSE, face.attributes & Attribute::Reverse);
    set_attribute(window, A_BLINK, face.attributes & Attribute::Blink);
    set_attribute(window, A_BOLD, face.attributes & Attribute::Bold);
    set_attribute(window, A_DIM, face.attributes & Attribute::Dim);
}

static sig_atomic_t resize_pending = 0;

void on_term_resize(int)
{
    resize_pending = 1;
    EventManager::instance().force_signal(0);
}

void on_sigint(int)
{
    // do nothing
}

NCursesUI::NCursesUI()
    : m_stdin_watcher{0, [this](FDWatcher&, EventMode mode) {
        if (m_input_callback)
            m_input_callback(mode);
      }},
      m_assistant(assistant_clippy)
{
    initscr();
    raw();
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

    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
    mouseinterval(0);
    // force enable report mouse position
    puts("\033[?1002h");
    update_dimensions();

    wrefresh(stdscr);
}

NCursesUI::~NCursesUI()
{
    puts("\033[?1002l");
    endwin();
    signal(SIGWINCH, SIG_DFL);
    signal(SIGINT, SIG_DFL);
}

void NCursesUI::redraw()
{
    redrawwin(m_window);
    wnoutrefresh(m_window);
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

void NCursesUI::refresh()
{
    if (m_dirty)
        redraw();
    m_dirty = false;
}

using Utf8Policy = utf8::InvalidPolicy::Pass;
using Utf8Iterator = utf8::iterator<const char*, Utf8Policy>;
void addutf8str(WINDOW* win, Utf8Iterator begin, Utf8Iterator end)
{
    waddnstr(win, begin.base(), end.base() - begin.base());
}

static CharCoord window_size(WINDOW* win)
{
    CharCoord size;
    getmaxyx(win, (int&)size.line, (int&)size.column);
    return size;
}

static CharCoord window_pos(WINDOW* win)
{
    CharCoord pos;
    getbegyx(win, (int&)pos.line, (int&)pos.column);
    return pos;
}

void NCursesUI::update_dimensions()
{
    m_dimensions = window_size(stdscr);

    if (m_window)
        delwin(m_window);
    m_window = (NCursesWin*)newwin((int)m_dimensions.line, (int)m_dimensions.column, 0, 0);

    --m_dimensions.line;
}

void NCursesUI::draw_line(const DisplayLine& line, CharCount col_index) const
{
    for (const DisplayAtom& atom : line)
    {
        set_face(m_window, atom.face);

        StringView content = atom.content();
        if (content.empty())
            continue;

        if (content[content.length()-1] == '\n' and
            content.char_length() - 1 < m_dimensions.column - col_index)
        {
            addutf8str(m_window, Utf8Iterator{content.begin()},
                                 Utf8Iterator{content.end()}-1);
            waddch(m_window, ' ');
        }
        else
        {
            Utf8Iterator begin{content.begin()}, end{content.end()};
            if (end - begin > m_dimensions.column - col_index)
                end = begin + (m_dimensions.column - col_index);
            addutf8str(m_window, begin, end);
            col_index += end - begin;
        }
    }
}

void NCursesUI::draw(const DisplayBuffer& display_buffer,
                     const DisplayLine& status_line,
                     const DisplayLine& mode_line)
{
    check_resize();

    LineCount line_index = m_status_on_top ? 1 : 0;
    for (const DisplayLine& line : display_buffer.lines())
    {
        wmove(m_window, (int)line_index, 0);
        wclrtoeol(m_window);
        draw_line(line, 0);
        ++line_index;
    }

    set_face(m_window, { Color::Blue, Color::Default });
    while (line_index < m_dimensions.line + (m_status_on_top ? 1 : 0))
    {
        wmove(m_window, (int)line_index++, 0);
        wclrtoeol(m_window);
        waddch(m_window, '~');
    }

    int status_line_pos = m_status_on_top ? 0 : (int)m_dimensions.line;
    wmove(m_window, status_line_pos, 0);
    wclrtoeol(m_window);
    draw_line(status_line, 0);
    const auto mode_len = mode_line.length();
    const auto remaining = m_dimensions.column - status_line.length();
    if (mode_len < remaining)
    {
        CharCount col = m_dimensions.column - mode_len;
        wmove(m_window, status_line_pos, (int)col);
        draw_line(mode_line, col);
    }
    else if (remaining > 2)
    {
        DisplayLine trimmed_mode_line = mode_line;
        trimmed_mode_line.trim(mode_len + 2 - remaining, remaining - 2, false);
        trimmed_mode_line.insert(trimmed_mode_line.begin(), { "…" });
        kak_assert(trimmed_mode_line.length() == remaining - 1);

        CharCount col = m_dimensions.column - remaining + 1;
        wmove(m_window, status_line_pos, (int)col);
        draw_line(trimmed_mode_line, col);
    }

    const char* tsl = tigetstr((char*)"tsl");
    const char* fsl = tigetstr((char*)"fsl");
    if (tsl != 0 and (ptrdiff_t)tsl != -1 and
        fsl != 0 and (ptrdiff_t)fsl != -1)
    {
        String title;
        for (auto& atom : mode_line)
            title += atom.content();
        title += " - Kakoune";
        printf("%s%s%s", tsl, title.c_str(), fsl);
    }

    m_dirty = true;
}

void NCursesUI::check_resize()
{
    if (resize_pending)
    {
        int fd = open("/dev/tty", O_RDWR);
        winsize ws;
        if (ioctl(fd, TIOCGWINSZ, (void*)&ws) == 0)
        {
            close(fd);
            resizeterm(ws.ws_row, ws.ws_col);
            update_dimensions();
        }
        ungetch(KEY_RESIZE);
        m_dirty = true;
        resize_pending = false;
    }
}

bool NCursesUI::is_key_available()
{
    check_resize();

    timeout(0);
    const int c = getch();
    if (c != ERR)
        ungetch(c);
    timeout(-1);
    return c != ERR;
}

Key NCursesUI::get_key()
{
    check_resize();

    const int c = getch();

    if (c == KEY_MOUSE)
    {
        MEVENT ev;
        if (getmouse(&ev) == OK)
        {
            auto wheel_down_mask = NCURSES_MOUSE_MASK(m_wheel_down_button, NCURSES_BUTTON_PRESSED);
            auto wheel_up_mask = NCURSES_MOUSE_MASK(m_wheel_up_button, NCURSES_BUTTON_PRESSED);

            CharCoord pos{ ev.y, ev.x };
            if ((ev.bstate & BUTTON1_PRESSED) == BUTTON1_PRESSED) return mouse_press(pos);
            if ((ev.bstate & BUTTON1_RELEASED) == BUTTON1_RELEASED) return mouse_release(pos);
            if ((ev.bstate & wheel_down_mask) == wheel_down_mask) return mouse_wheel_down(pos);
            if ((ev.bstate & wheel_up_mask) == wheel_up_mask) return mouse_wheel_up(pos);
            else return mouse_pos(pos);
        }
    }

    if (c > 0 and c < 27)
    {
        if (c == CTRL('l'))
           redrawwin(stdscr);
        if (c == CTRL('z'))
        {
            raise(SIGTSTP);
            return Key::Invalid;
        }
        return ctrl(Codepoint(c) - 1 + 'a');
    }
    else if (c == 27)
    {
        timeout(0);
        const Codepoint new_c = getch();
        timeout(-1);
        if (new_c != ERR)
        {
            if (new_c > 0 and new_c < 27)
                return ctrlalt(Codepoint(new_c) - 1 + 'a');
            return alt(new_c);
        }
        else
            return Key::Escape;
    }
    else switch (c)
    {
    case KEY_BACKSPACE: case 127: return Key::Backspace;
    case KEY_DC: return Key::Delete;
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

    for (int i = 0; i < 12; ++i)
    {
        if (c == KEY_F(i+1))
            return Key::F1 + i;
    }

    if (c >= 0 and c < 256)
    {
       ungetch(c);
       struct getch_iterator
       {
            int operator*() { return getch(); }
            getch_iterator& operator++() { return *this; }
            getch_iterator& operator++(int) { return *this; }
            bool operator== (const getch_iterator&) const { return false; }
       };
       return utf8::codepoint(getch_iterator{}, getch_iterator{});
    }
    return Key::Invalid;
}

template<typename T>
T div_round_up(T a, T b)
{
    return (a - T(1)) / b + T(1);
}

void NCursesUI::draw_menu()
{
    // menu show may have not created the window if it did not fit.
    // so be tolerant.
    if (not m_menu_win)
        return;

    const auto menu_fg = get_color_pair(m_menu_fg);
    const auto menu_bg = get_color_pair(m_menu_bg);

    wattron(m_menu_win, COLOR_PAIR(menu_bg));
    wbkgdset(m_menu_win, COLOR_PAIR(menu_bg));

    const int item_count = (int)m_items.size();
    const LineCount menu_lines = div_round_up(item_count, m_menu_columns);
    const CharCoord win_size = window_size(m_menu_win);
    const LineCount& win_height = win_size.line;
    kak_assert(win_height <= menu_lines);

    const CharCount column_width = (win_size.column - 1) / m_menu_columns;

    const LineCount mark_height = min(div_round_up(sq(win_height), menu_lines),
                                      win_height);
    const LineCount mark_line = (win_height - mark_height) * m_menu_top_line /
                                max(1_line, menu_lines - win_height);
    for (auto line = 0_line; line < win_height; ++line)
    {
        wmove(m_menu_win, (int)line, 0);
        for (int col = 0; col < m_menu_columns; ++col)
        {
            const int item_idx = (int)(m_menu_top_line + line) * m_menu_columns
                                 + col;
            if (item_idx >= item_count)
                break;
            if (item_idx == m_selected_item)
                wattron(m_menu_win, COLOR_PAIR(menu_fg));

            StringView item = m_items[item_idx];
            auto begin = item.begin();
            auto end = utf8::advance(begin, item.end(), column_width);
            addutf8str(m_menu_win, begin, end);
            const CharCount pad = column_width - utf8::distance(begin, end);
            waddstr(m_menu_win, String{' ' COMMA pad}.c_str());
            wattron(m_menu_win, COLOR_PAIR(menu_bg));
        }
        const bool is_mark = line >= mark_line and
                             line < mark_line + mark_height;
        wclrtoeol(m_menu_win);
        wmove(m_menu_win, (int)line, (int)win_size.column - 1);
        wattron(m_menu_win, COLOR_PAIR(menu_bg));
        waddstr(m_menu_win, is_mark ? "█" : "░");
    }
    m_dirty = true;
}

void NCursesUI::menu_show(ConstArrayView<String> items,
                          CharCoord anchor, Face fg, Face bg,
                          MenuStyle style)
{
    if (m_menu_win)
        delwin(m_menu_win);
    m_items.clear();

    m_menu_fg = fg;
    m_menu_bg = bg;

    if (style == MenuStyle::Prompt)
        anchor = CharCoord{m_status_on_top ? 0_line : m_dimensions.line, 0};
    else if (m_status_on_top)
        anchor.line += 1;

    CharCoord maxsize = window_size(stdscr);
    maxsize.column -= anchor.column;
    if (maxsize.column <= 2)
        return;

    const int item_count = items.size();
    m_items.reserve(item_count);
    CharCount longest = 0;
    const CharCount maxlen = min((int)maxsize.column-2, 200);
    for (auto& item : items)
    {
        m_items.push_back(item.substr(0_char, maxlen).str());
        longest = max(longest, m_items.back().char_length());
    }
    longest += 1;

    const bool is_prompt = style == MenuStyle::Prompt;
    m_menu_columns = is_prompt ? (int)((maxsize.column - 1) / longest) : 1;

    int height = min(10, div_round_up(item_count, m_menu_columns));

    int line = (int)anchor.line + 1;
    if (line + height >= (int)maxsize.line)
        line = (int)anchor.line - height;
    m_selected_item = item_count;
    m_menu_top_line = 0;

    int width = is_prompt ? (int)maxsize.column : (int)longest;
    m_menu_win = (NCursesWin*)newwin(height, width, line, (int)anchor.column);
    draw_menu();
}

void NCursesUI::menu_select(int selected)
{
    const int item_count = m_items.size();
    const LineCount menu_lines = div_round_up(item_count, m_menu_columns);
    if (selected < 0 or selected >= item_count)
    {
        m_selected_item = -1;
        m_menu_top_line = 0;
    }
    else
    {
        m_selected_item = selected;
        const LineCount selected_line = m_selected_item / m_menu_columns;
        const LineCount win_height = window_size(m_menu_win).line;
        kak_assert(menu_lines >= win_height);
        if (selected_line < m_menu_top_line)
            m_menu_top_line = selected_line;
        if (selected_line >= m_menu_top_line + win_height)
            m_menu_top_line = min(selected_line, menu_lines - win_height);
    }
    draw_menu();
}

void NCursesUI::menu_hide()
{
    if (not m_menu_win)
        return;
    m_items.clear();
    delwin(m_menu_win);
    m_menu_win = nullptr;
    m_dirty = true;
}

static CharCoord compute_needed_size(StringView str)
{
    CharCoord res{1,0};
    CharCount line_len = 0;
    for (Utf8Iterator begin{str.begin()}, end{str.end()};
         begin != end; ++begin)
    {
        if (*begin == '\n')
        {
            // ignore last '\n', no need to show an empty line
            if (begin+1 == end)
                break;

            res.column = max(res.column, line_len);
            line_len = 0;
            ++res.line;
        }
        else
        {
            ++line_len;
            res.column = max(res.column, line_len);
        }
    }
    return res;
}

static CharCoord compute_pos(CharCoord anchor, CharCoord size,
                             WINDOW* opt_window_to_avoid = nullptr,
                             bool prefer_above = false)
{
    CharCoord scrsize = window_size(stdscr);
    CharCoord pos;
    if (prefer_above)
    {
        pos = anchor - CharCoord{size.line};
        if (pos.line < 0)
            prefer_above = false;
    }
    if (not prefer_above)
    {
        pos = anchor + CharCoord{1_line};
        if (pos.line + size.line >= scrsize.line)
            pos.line = max(0_line, anchor.line - size.line);
    }
    if (pos.column + size.column >= scrsize.column)
        pos.column = max(0_char, scrsize.column - size.column);

    if (opt_window_to_avoid)
    {
        CharCoord winbeg = window_pos(opt_window_to_avoid);
        CharCoord winend = winbeg + window_size(opt_window_to_avoid);

        CharCoord end = pos + size;

        // check intersection
        if (not (end.line < winbeg.line or end.column < winbeg.column or
                 pos.line > winend.line or pos.column > winend.column))
        {
            pos.line = min(winbeg.line, anchor.line) - size.line;
            // if above does not work, try below
            if (pos.line < 0)
                pos.line = max(winend.line, anchor.line);
        }
    }

    return pos;
}

String make_info_box(StringView title, StringView message, CharCount max_width,
                     ConstArrayView<StringView> assistant)
{
    CharCoord assistant_size;
    if (not assistant.empty())
        assistant_size = { (int)assistant.size(), assistant[0].char_length() };

    const CharCount max_bubble_width = max_width - assistant_size.column - 6;
    Vector<StringView> lines = wrap_lines(message, max_bubble_width);

    CharCount bubble_width = title.char_length() + 2;
    for (auto& line : lines)
        bubble_width = max(bubble_width, line.char_length());

    String result;
    auto line_count = max(assistant_size.line-1,
                          LineCount{(int)lines.size()} + 2);
    for (LineCount i = 0; i < line_count; ++i)
    {
        constexpr Codepoint dash{L'─'};
        if (not assistant.empty())
            result += assistant[min((int)i, (int)assistant_size.line-1)];
        if (i == 0)
        {
            if (title.empty())
                result += "╭─" + String{dash, bubble_width} + "─╮";
            else
            {
                auto dash_count = bubble_width - title.char_length() - 2;
                String left{dash, dash_count / 2};
                String right{dash, dash_count - dash_count / 2};
                result += "╭─" + left + "┤" + title +"├" + right +"─╮";
            }
        }
        else if (i < lines.size() + 1)
        {
            auto& line = lines[(int)i - 1];
            const CharCount padding = bubble_width - line.char_length();
            result += "│ " + line + String{' ', padding} + " │";
        }
        else if (i == lines.size() + 1)
            result += "╰─" + String(dash, bubble_width) + "─╯";

        result += "\n";
    }
    return result;
}

void NCursesUI::info_show(StringView title, StringView content,
                          CharCoord anchor, Face face, InfoStyle style)
{
    if (m_info_win)
        delwin(m_info_win);

    StringView info_box = content;
    String fancy_info_box;
    if (style == InfoStyle::Prompt)
    {
        fancy_info_box = make_info_box(title, content, m_dimensions.column,
                                       m_assistant);
        info_box = fancy_info_box;
        anchor = CharCoord{m_status_on_top ? 0 : m_dimensions.line,
                           m_dimensions.column-1};
    }
    else
    {
        if (m_status_on_top)
            anchor.line += 1;
        CharCount col = anchor.column;
        if (style == InfoStyle::MenuDoc and m_menu_win)
            col = window_pos(m_menu_win).column + window_size(m_menu_win).column;

        for (auto& line : wrap_lines(content, m_dimensions.column - col))
            fancy_info_box += line + "\n";
        info_box = fancy_info_box;
    }

    CharCoord size = compute_needed_size(info_box);
    CharCoord pos;
    if (style == InfoStyle::MenuDoc and m_menu_win)
        pos = window_pos(m_menu_win) +
              CharCoord{0_line, window_size(m_menu_win).column};
    else
        pos = compute_pos(anchor, size, m_menu_win, style == InfoStyle::InlineAbove);

    m_info_win = (NCursesWin*)newwin((int)size.line, (int)size.column,
                                     (int)pos.line,  (int)pos.column);

    wbkgd(m_info_win, COLOR_PAIR(get_color_pair(face)));
    int line = 0;
    auto it = info_box.begin(), end = info_box.end();
    while (true)
    {
        wmove(m_info_win, line++, 0);
        auto eol = std::find_if(it, end, [](char c) { return c == '\n'; });
        addutf8str(m_info_win, Utf8Iterator(it), Utf8Iterator(eol));
        if (eol == end)
           break;
        it = eol + 1;
    }
    m_dirty = true;
}

void NCursesUI::info_hide()
{
    if (not m_info_win)
        return;
    delwin(m_info_win);
    m_info_win = nullptr;
    m_dirty = true;
}

CharCoord NCursesUI::dimensions()
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

void NCursesUI::set_ui_options(const Options& options)
{
    {
        auto it = options.find("ncurses_assistant");
        if (it == options.end())
            m_assistant = assistant_clippy;
        else if (it->second == "cat")
            m_assistant = assistant_cat;
        else if (it->second == "clippy")
            m_assistant = assistant_clippy;
        else if (it->second == "none" or it->second == "off")
            m_assistant = ConstArrayView<StringView>{};
    }

    {
        auto it = options.find("ncurses_status_on_top");
        if (it == options.end())
            m_status_on_top = false;
        else
            m_status_on_top = it->second == "yes" or it->second == "true";
    }

    {
        auto wheel_down_it = options.find("ncurses_wheel_down_button");
        if (wheel_down_it == options.end())
            m_wheel_down_button = 2;
        else try {
            m_wheel_down_button = str_to_int(wheel_down_it->second);;
        } catch(...) {}

        auto wheel_up_it = options.find("ncurses_wheel_up_button");
        if (wheel_up_it == options.end())
            m_wheel_up_button = 4;
        else try {
            m_wheel_up_button = str_to_int(wheel_up_it->second);;
        } catch(...) {}
    }
}

}
