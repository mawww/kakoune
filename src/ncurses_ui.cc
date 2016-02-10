#include "ncurses_ui.hh"

#include "containers.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "file.hh"
#include "keys.hh"
#include "register_manager.hh"
#include "utf8_iterator.hh"

#include <algorithm>

#define NCURSES_OPAQUE 0
#define NCURSES_INTERNALS

#include <ncurses.h>

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

constexpr char control(char c) { return c & 037; }

namespace Kakoune
{

using std::min;
using std::max;

struct NCursesWin : WINDOW {};

static void set_attribute(WINDOW* window, int attribute, bool on)
{
    if (on)
        wattron(window, attribute);
    else
        wattroff(window, attribute);
}

template<typename T> T sq(T x) { return x * x; }

constexpr struct { unsigned char r, g, b; } builtin_colors[] = {
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

int NCursesUI::get_color(Color color)
{
    auto it = m_colors.find(color);
    if (it != m_colors.end())
        return it->second;
    else if (can_change_color() and COLORS > 16)
    {
        kak_assert(color.color == Color::RGB);
        if (m_next_color > COLORS)
            m_next_color = 16;
        init_color(m_next_color,
                   color.r * 1000 / 255,
                   color.g * 1000 / 255,
                   color.b * 1000 / 255);
        m_colors[color] = m_next_color;
        return m_next_color++;
    }
    else
    {
        kak_assert(color.color == Color::RGB);
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

int NCursesUI::get_color_pair(const Face& face)
{
    static int next_pair = 1;

    ColorPair colors{face.fg, face.bg};
    auto it = m_colorpairs.find(colors);
    if (it != m_colorpairs.end())
        return it->second;
    else
    {
        init_pair(next_pair, get_color(face.fg), get_color(face.bg));
        m_colorpairs[colors] = next_pair;
        return next_pair++;
    }
}

void NCursesUI::set_face(NCursesWin* window, Face face, const Face& default_face)
{
    static int current_pair = -1;

    if (current_pair != -1)
        wattroff(window, COLOR_PAIR(current_pair));

    if (face.fg == Color::Default)
        face.fg = default_face.fg;
    if (face.bg == Color::Default)
        face.bg = default_face.bg;

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
    #if defined(A_ITALIC)
    set_attribute(window, A_ITALIC, face.attributes & Attribute::Italic);
    #endif
}

static sig_atomic_t resize_pending = 0;

void on_term_resize(int)
{
    resize_pending = 1;
    EventManager::instance().force_signal(0);
}

NCursesUI::NCursesUI()
    : m_stdin_watcher{0, [this](FDWatcher&, EventMode mode) {
        if (m_input_callback)
            m_input_callback(mode);
      }},
      m_assistant(assistant::clippy),
      m_colors{
        { Color::Default, -1 },
        { Color::Black,   COLOR_BLACK },
        { Color::Red,     COLOR_RED },
        { Color::Green,   COLOR_GREEN },
        { Color::Yellow,  COLOR_YELLOW },
        { Color::Blue,    COLOR_BLUE },
        { Color::Magenta, COLOR_MAGENTA },
        { Color::Cyan,    COLOR_CYAN },
        { Color::White,   COLOR_WHITE },
      }
{
    initscr();
    raw();
    noecho();
    nonl();
    curs_set(0);
    start_color();
    use_default_colors();
    set_escdelay(25);

    enable_mouse(true);

    set_signal_handler(SIGWINCH, on_term_resize);
    set_signal_handler(SIGCONT, on_term_resize);

    check_resize(true);

    redraw();
}

NCursesUI::~NCursesUI()
{
    enable_mouse(false);
    endwin();
    set_signal_handler(SIGWINCH, SIG_DFL);
    set_signal_handler(SIGCONT, SIG_DFL);
}

void NCursesUI::Window::create(const CharCoord& p, const CharCoord& s)
{
    pos = p;
    size = s;
    win = (NCursesWin*)newpad((int)size.line, (int)size.column);
}

void NCursesUI::Window::destroy()
{
    delwin(win);
    win = nullptr;
    pos = CharCoord{};
    size = CharCoord{};
}

void NCursesUI::Window::refresh()
{
    if (not win)
        return;

    redrawwin(win);
    CharCoord max_pos = pos + size - CharCoord{1,1};
    pnoutrefresh(win, 0, 0, (int)pos.line, (int)pos.column,
                 (int)max_pos.line, (int)max_pos.column);
}

void NCursesUI::redraw()
{
    pnoutrefresh(m_window, 0, 0, 0, 0,
                 (int)m_dimensions.line + 1, (int)m_dimensions.column);
    m_menu.refresh();
    m_info.refresh();
    doupdate();
}

void NCursesUI::refresh()
{
    if (m_dirty)
        redraw();
    m_dirty = false;
}

void add_str(WINDOW* win, StringView str)
{
    waddnstr(win, str.begin(), (int)str.length());
}

void NCursesUI::draw_line(NCursesWin* window, const DisplayLine& line,
                          CharCount col_index, CharCount max_column,
                          const Face& default_face)
{
    for (const DisplayAtom& atom : line)
    {
        set_face(window, atom.face, default_face);

        StringView content = atom.content();
        if (content.empty())
            continue;

        const auto remaining_columns = max_column - col_index;
        if (content.back() == '\n' and
            content.char_length() - 1 < remaining_columns)
        {
            add_str(window, content.substr(0, content.length()-1));
            waddch(window, ' ');
        }
        else
        {
            content = content.substr(0_char, remaining_columns);
            add_str(window, content);
            col_index += content.char_length();
        }
    }
}

void NCursesUI::draw(const DisplayBuffer& display_buffer,
                     const Face& default_face)
{
    wbkgdset(m_window, COLOR_PAIR(get_color_pair(default_face)));

    check_resize();

    LineCount line_index = m_status_on_top ? 1 : 0;
    for (const DisplayLine& line : display_buffer.lines())
    {
        wmove(m_window, (int)line_index, 0);
        wclrtoeol(m_window);
        draw_line(m_window, line, 0, m_dimensions.column, default_face);
        ++line_index;
    }

    set_face(m_window, { Color::Blue, Color::Default }, default_face);
    while (line_index < m_dimensions.line + (m_status_on_top ? 1 : 0))
    {
        wmove(m_window, (int)line_index++, 0);
        wclrtoeol(m_window);
        waddch(m_window, '~');
    }

    m_dirty = true;
}

void NCursesUI::draw_status(const DisplayLine& status_line,
                            const DisplayLine& mode_line,
                            const Face& default_face)
{
    const int status_line_pos = m_status_on_top ? 0 : (int)m_dimensions.line;
    wmove(m_window, status_line_pos, 0);

    wbkgdset(m_window, COLOR_PAIR(get_color_pair(default_face)));
    wclrtoeol(m_window);

    draw_line(m_window, status_line, 0, m_dimensions.column, default_face);

    const auto mode_len = mode_line.length();
    const auto remaining = m_dimensions.column - status_line.length();
    if (mode_len < remaining)
    {
        CharCount col = m_dimensions.column - mode_len;
        wmove(m_window, status_line_pos, (int)col);
        draw_line(m_window, mode_line, col, m_dimensions.column, default_face);
    }
    else if (remaining > 2)
    {
        DisplayLine trimmed_mode_line = mode_line;
        trimmed_mode_line.trim(mode_len + 2 - remaining, remaining - 2, false);
        trimmed_mode_line.insert(trimmed_mode_line.begin(), { "…" });
        kak_assert(trimmed_mode_line.length() == remaining - 1);

        CharCount col = m_dimensions.column - remaining + 1;
        wmove(m_window, status_line_pos, (int)col);
        draw_line(m_window, trimmed_mode_line, col, m_dimensions.column, default_face);
    }

    if (m_set_title)
    {
        String title = "\033]2;";
        for (auto& atom : mode_line)
            title += atom.content();
        title += " - Kakoune\007";
        write_stdout(title);
    }

    m_dirty = true;
}

void NCursesUI::check_resize(bool force)
{
    if (not force and not resize_pending)
        return;

    resize_pending = 0;

    const int fd = open("/dev/tty", O_RDWR);
    auto close_fd = on_scope_end([fd]{ close(fd); });
    winsize ws;
    if (ioctl(fd, TIOCGWINSZ, (void*)&ws) == 0)
    {
        const bool info = (bool)m_info;
        const bool menu = (bool)m_menu;
        if (m_window) delwin(m_window);
        if (info) m_info.destroy();
        if (menu) m_menu.destroy();

        resize_term(ws.ws_row, ws.ws_col);

        m_window = (NCursesWin*)newpad(ws.ws_row, ws.ws_col);
        intrflush(m_window, false);
        keypad(m_window, true);

        m_dimensions = CharCoord{ws.ws_row-1, ws.ws_col};

        if (char* csr = tigetstr((char*)"csr"))
            putp(csr);

        if (menu)
        {
            auto items = std::move(m_items);
            menu_show(items, m_menu_anchor, m_menu_fg, m_menu_bg, m_menu_style);
        }
        if (info)
            info_show(m_info_title, m_info_content, m_info_anchor, m_info_face, m_info_style);
    }
    else
        kak_assert(false);

    ungetch(KEY_RESIZE);
    clearok(curscr, true);
    werase(curscr);
}

bool NCursesUI::is_key_available()
{
    check_resize();

    wtimeout(m_window, 0);
    const int c = wgetch(m_window);
    if (c != ERR)
        ungetch(c);
    wtimeout(m_window, -1);
    return c != ERR;
}

Key NCursesUI::get_key()
{
    check_resize();

    const int c = wgetch(m_window);

    if (c == KEY_MOUSE)
    {
        MEVENT ev;
        if (getmouse(&ev) == OK)
        {
            CharCoord pos{ ev.y - (m_status_on_top ? 1 : 0), ev.x };
            if (BUTTON_PRESS(ev.bstate, 1)) return mouse_press(pos);
            if (BUTTON_RELEASE(ev.bstate, 1)) return mouse_release(pos);
            if (BUTTON_PRESS(ev.bstate, m_wheel_down_button)) return mouse_wheel_down(pos);
            if (BUTTON_PRESS(ev.bstate, m_wheel_up_button)) return mouse_wheel_up(pos);
            return mouse_pos(pos);
        }
    }

    if (c > 0 and c < 27)
    {
        if (c == control('l'))
        {
           redrawwin(m_window);
           redraw();
        }
        if (c == control('z'))
        {
            raise(SIGTSTP);
            return Key::Invalid;
        }
        return ctrl(Codepoint(c) - 1 + 'a');
    }
    else if (c == 27)
    {
        wtimeout(m_window, 0);
        const Codepoint new_c = wgetch(m_window);
        if (new_c == '[') // potential CSI
        {
            const Codepoint csi_val = wgetch(m_window);
            switch (csi_val)
            {
                case 'I': return Key::FocusIn;
                case 'O': return Key::FocusOut;
                default: break; // nothing
            }
        }
        wtimeout(m_window, -1);
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
    case KEY_RESIZE: return resize(m_dimensions);
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
           getch_iterator(WINDOW* win) : window(win) {}
           int operator*() { return wgetch(window); }
           getch_iterator& operator++() { return *this; }
           getch_iterator& operator++(int) { return *this; }
           bool operator== (const getch_iterator&) const { return false; }

            WINDOW* window;
       };
       return utf8::codepoint(getch_iterator{m_window}, getch_iterator{m_window});
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
    if (not m_menu)
        return;

    const auto menu_bg = get_color_pair(m_menu_bg);
    wattron(m_menu.win, COLOR_PAIR(menu_bg));
    wbkgdset(m_menu.win, COLOR_PAIR(menu_bg));

    const int item_count = (int)m_items.size();
    const LineCount menu_lines = div_round_up(item_count, m_menu_columns);
    const LineCount& win_height = m_menu.size.line;
    kak_assert(win_height <= menu_lines);

    const CharCount column_width = (m_menu.size.column - 1) / m_menu_columns;

    const LineCount mark_height = min(div_round_up(sq(win_height), menu_lines),
                                      win_height);
    const LineCount mark_line = (win_height - mark_height) * m_menu_top_line /
                                max(1_line, menu_lines - win_height);
    for (auto line = 0_line; line < win_height; ++line)
    {
        wmove(m_menu.win, (int)line, 0);
        for (int col = 0; col < m_menu_columns; ++col)
        {
            const int item_idx = (int)(m_menu_top_line + line) * m_menu_columns
                                 + col;
            if (item_idx >= item_count)
                break;

            const DisplayLine& item = m_items[item_idx];
            draw_line(m_menu.win, item, 0, column_width,
                      item_idx == m_selected_item ? m_menu_fg : m_menu_bg);
            const CharCount pad = column_width - item.length();
            add_str(m_menu.win, String{' ' COMMA pad});
        }
        const bool is_mark = line >= mark_line and
                             line < mark_line + mark_height;
        wclrtoeol(m_menu.win);
        wmove(m_menu.win, (int)line, (int)m_menu.size.column - 1);
        wattron(m_menu.win, COLOR_PAIR(menu_bg));
        add_str(m_menu.win, is_mark ? "█" : "░");
    }
    m_dirty = true;
}

void NCursesUI::menu_show(ConstArrayView<DisplayLine> items,
                          CharCoord anchor, Face fg, Face bg,
                          MenuStyle style)
{
    menu_hide();

    m_menu_fg = fg;
    m_menu_bg = bg;
    m_menu_style = style;
    m_menu_anchor = anchor;

    if (style == MenuStyle::Prompt)
        anchor = CharCoord{m_status_on_top ? 0_line : m_dimensions.line, 0};
    else if (m_status_on_top)
        anchor.line += 1;

    CharCoord maxsize = m_dimensions;
    maxsize.column -= anchor.column;
    if (maxsize.column <= 2)
        return;

    const int item_count = items.size();
    m_items.clear(); // make sure it is empty
    m_items.reserve(item_count);
    CharCount longest = 1;
    for (auto& item : items)
        longest = max(longest, item.length());

    const bool is_prompt = style == MenuStyle::Prompt;
    m_menu_columns = is_prompt ? max((int)((maxsize.column-1) / (longest+1)), 1) : 1;

    CharCount maxlen = maxsize.column-1;
    if (m_menu_columns > 1 and item_count > 1)
        maxlen = maxlen / m_menu_columns - 1;

    for (auto& item : items)
    {
        m_items.push_back(item);
        m_items.back().trim(0, maxlen, false);
        kak_assert(m_items.back().length() <= maxlen);
    }

    int height = min(10, div_round_up(item_count, m_menu_columns));

    int line = (int)anchor.line + 1;
    if (line + height >= (int)maxsize.line)
        line = (int)anchor.line - height;
    m_selected_item = item_count;
    m_menu_top_line = 0;

    auto width = is_prompt ? maxsize.column : min(longest+1, maxsize.column);
    m_menu.create({line, anchor.column}, {height, width});
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
        const LineCount win_height = m_menu.size.line;
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
    if (not m_menu)
        return;
    m_items.clear();
    mark_dirty(m_menu);
    m_menu.destroy();
    m_dirty = true;
}

static CharCoord compute_needed_size(StringView str)
{
    CharCoord res{1,0};
    CharCount line_len = 0;
    for (auto it = str.begin(), end = str.end();
         it != end; it = utf8::next(it, end))
    {
        if (*it == '\n')
        {
            // ignore last '\n', no need to show an empty line
            if (it+1 == end)
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
                             NCursesUI::Rect rect, NCursesUI::Rect to_avoid,
                             bool prefer_above)
{
    CharCoord pos;
    if (prefer_above)
    {
        pos = anchor - CharCoord{size.line};
        if (pos.line < 0)
            prefer_above = false;
    }
    auto rect_end = rect.pos + rect.size;
    if (not prefer_above)
    {
        pos = anchor + CharCoord{1_line};
        if (pos.line + size.line > rect_end.line)
            pos.line = max(rect.pos.line, anchor.line - size.line);
    }
    if (pos.column + size.column > rect_end.column)
        pos.column = max(rect.pos.column, rect_end.column - size.column);

    if (to_avoid.size != CharCoord{})
    {
        CharCoord to_avoid_end = to_avoid.pos + to_avoid.size;

        CharCoord end = pos + size;

        // check intersection
        if (not (end.line < to_avoid.pos.line or end.column < to_avoid.pos.column or
                 pos.line > to_avoid_end.line or pos.column > to_avoid_end.column))
        {
            pos.line = min(to_avoid.pos.line, anchor.line) - size.line;
            // if above does not work, try below
            if (pos.line < 0)
                pos.line = max(to_avoid_end.line, anchor.line);
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

    String result;

    const CharCount max_bubble_width = max_width - assistant_size.column - 6;
    if (max_bubble_width < 4)
        return result;

    Vector<StringView> lines = wrap_lines(message, max_bubble_width);

    CharCount bubble_width = title.char_length() + 2;
    for (auto& line : lines)
        bubble_width = max(bubble_width, line.char_length());

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
    info_hide();

    m_info_title = title.str();
    m_info_content = content.str();
    m_info_anchor = anchor;
    m_info_face = face;
    m_info_style = style;

    String info_box;
    if (style == InfoStyle::Prompt)
    {
        info_box = make_info_box(m_info_title, m_info_content,
                                 m_dimensions.column, m_assistant);
        anchor = CharCoord{m_status_on_top ? 0 : m_dimensions.line,
                           m_dimensions.column-1};
    }
    else
    {
        if (m_status_on_top)
            anchor.line += 1;
        CharCount col = anchor.column;
        if (style == InfoStyle::MenuDoc and m_menu)
            col = m_menu.pos.column + m_menu.size.column;

        const CharCount max_width = m_dimensions.column - col;
        if (max_width < 4)
            return;

        for (auto& line : wrap_lines(m_info_content, max_width))
            info_box += line + "\n";
    }

    CharCoord size = compute_needed_size(info_box), pos;
    const Rect rect = {m_status_on_top ? 1_line : 0_line, m_dimensions};
    if (style == InfoStyle::MenuDoc and m_menu)
        pos = m_menu.pos + CharCoord{0_line, m_menu.size.column};
    else
        pos = compute_pos(anchor, size, rect, m_menu, style == InfoStyle::InlineAbove);

    // The info box does not fit
    if (pos < rect.pos or pos + size > rect.pos + rect.size)
        return;

    m_info.create(pos, size);

    wbkgd(m_info.win, COLOR_PAIR(get_color_pair(face)));
    int line = 0;
    auto it = info_box.begin(), end = info_box.end();
    while (true)
    {
        wmove(m_info.win, line++, 0);
        auto eol = std::find_if(it, end, [](char c) { return c == '\n'; });
        add_str(m_info.win, {it, eol});
        if (eol == end)
           break;
        it = eol + 1;
    }
    m_dirty = true;
}

void NCursesUI::info_hide()
{
    if (not m_info)
        return;
    mark_dirty(m_info);
    m_info.destroy();
    m_dirty = true;
}

void NCursesUI::mark_dirty(const Window& win)
{
    wredrawln(m_window, (int)win.pos.line, (int)win.size.line);
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

void NCursesUI::enable_mouse(bool enabled)
{
    if (enabled == m_mouse_enabled)
        return;

    m_mouse_enabled = enabled;
    if (enabled)
    {
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
        mouseinterval(0);
        // force enable report mouse position
        puts("\033[?1002h");
        // force enable report focus events
        puts("\033[?1004h");
    }
    else
    {
        mousemask(0, nullptr);
        puts("\033[?1004l");
        puts("\033[?1002l");
    }
}

void NCursesUI::set_ui_options(const Options& options)
{
    {
        auto it = options.find("ncurses_assistant");
        if (it == options.end())
            m_assistant = assistant::clippy;
        else if (it->value == "none" or it->value == "off")
            m_assistant = ConstArrayView<StringView>{};
        else
        {
            if (it->value == "clippy") m_assistant = assistant::clippy;
            else if (it->value == "cat") m_assistant = assistant::cat;
            else if (it->value == "owl") m_assistant = assistant::owl;
            else if (it->value == "gnu") m_assistant = assistant::gnu;
            else if (it->value == "dilbert") m_assistant = assistant::dilbert;
            else if (it->value == "tux") m_assistant = assistant::tux;
        }
    }

    {
        auto it = options.find("ncurses_status_on_top");
        m_status_on_top = it != options.end() and
            (it->value == "yes" or it->value == "true");
    }

    {
        auto it = options.find("ncurses_set_title");
        m_set_title = it == options.end() or
            (it->value == "yes" or it->value == "true");
    }

    {
        auto enable_mouse_it = options.find("ncurses_enable_mouse");
        enable_mouse(enable_mouse_it == options.end() or
                     enable_mouse_it->value == "yes" or
                     enable_mouse_it->value == "true");

        auto wheel_up_it = options.find("ncurses_wheel_up_button");
        m_wheel_up_button = wheel_up_it != options.end() ?
            str_to_int_ifp(wheel_up_it->value).value_or(4) : 4;

        auto wheel_down_it = options.find("ncurses_wheel_down_button");
        m_wheel_down_button = wheel_down_it != options.end() ?
            str_to_int_ifp(wheel_down_it->value).value_or(5) : 5;
    }
}

}
