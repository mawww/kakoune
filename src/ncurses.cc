#include "ncurses.hh"

#include "display_buffer.hh"
#include "register_manager.hh"

#include "utf8_iterator.hh"

#include <map>

#define CTRL(x) x - 'a' + 1

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


NCursesUI::NCursesUI()
{
    //setlocale(LC_CTYPE, "");
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
}

NCursesUI::~NCursesUI()
{
    endwin();
}

static void redraw(WINDOW* menu_win)
{
    wnoutrefresh(stdscr);
    if (menu_win)
    {
        redrawwin(menu_win);
        wnoutrefresh(menu_win);
    }
    doupdate();
}
using Utf8Policy = utf8::InvalidBytePolicy::Pass;
using Utf8Iterator = utf8::utf8_iterator<String::iterator, Utf8Policy>;
void addutf8str(Utf8Iterator begin, Utf8Iterator end)
{
    while (begin != end)
        addch(*begin++);
}

void NCursesUI::draw(const DisplayBuffer& display_buffer,
                     const String& status_line)
{
    int max_x,max_y;
    getmaxyx(stdscr, max_y, max_x);
    max_y -= 1;
    int status_y = max_y;

    int line_index = 0;
    for (const DisplayLine& line : display_buffer.lines())
    {
        move(line_index, 0);
        clrtoeol();
        for (const DisplayAtom& atom : line)
        {
            set_attribute(A_UNDERLINE, atom.attribute & Underline);
            set_attribute(A_REVERSE, atom.attribute & Reverse);
            set_attribute(A_BLINK, atom.attribute & Blink);
            set_attribute(A_BOLD, atom.attribute & Bold);

            set_color(atom.fg_color, atom.bg_color);

            String content = atom.content.content();
            int y,x;
            getyx(stdscr, y,x);
            if (content[content.length()-1] == '\n' and content.length() - 1 < max_x - x)
            {
                addutf8str(Utf8Iterator(content.begin()), Utf8Iterator(content.end())-1);
                addch(' ');
            }
            else
            {
                Utf8Iterator begin(content.begin()), end(content.end());
                if (end - begin > max_x - x)
                    end = begin + (max_x - x);
                addutf8str(begin, end);
            }
        }
        ++line_index;
    }

    set_attribute(A_UNDERLINE, 0);
    set_attribute(A_REVERSE, 0);
    set_attribute(A_BLINK, 0);
    set_attribute(A_BOLD, 0);
    set_color(Color::Blue, Color::Black);
    for (;line_index < max_y; ++line_index)
    {
        move(line_index, 0);
        clrtoeol();
        addch('~');
    }

    set_color(Color::Cyan, Color::Black);
    static int last_status_length = 0;
    move(status_y, max_x - last_status_length);
    clrtoeol();
    move(status_y, max_x - (int)status_line.length());
    addstr(status_line.c_str());
    last_status_length = (int)status_line.length();

    redraw(m_menu_win);
}

struct getch_iterator
{
    int operator*() { return getch(); }
    getch_iterator& operator++() { return *this; }
    getch_iterator& operator++(int) { return *this; }
};

Key NCursesUI::get_key()
{
    const unsigned c = getch();
    if (c > 0 and c < 27)
    {
        return {Key::Modifiers::Control, c - 1 + 'a'};
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
    else switch (c)
    {
    case KEY_BACKSPACE: return Key::Backspace;
    case KEY_UP: return Key::Up;
    case KEY_DOWN: return Key::Down;
    case KEY_LEFT: return Key::Left;
    case KEY_RIGHT: return Key::Right;
    case KEY_PPAGE: return Key::PageUp;
    case KEY_NPAGE: return Key::PageDown;
    case KEY_BTAB: return Key::BackTab;
    }

    if (c < 256)
    {
       ungetch(c);
       return utf8::codepoint(getch_iterator{});
    }
    return Key::Invalid;
}

void NCursesUI::print_status(const String& status, CharCount cursor_pos)
{
    int x,y;
    getmaxyx(stdscr, y, x);
    move(y-1, 0);
    clrtoeol();
    if (cursor_pos == -1)
       addutf8str(status.begin(), status.end());
    else
    {
        auto cursor_it = utf8::advance(status.begin(), status.end(), (int)cursor_pos);
        auto end = status.end();
        addutf8str(status.begin(), cursor_it);
        set_attribute(A_REVERSE, 1);
        addch((cursor_it == end) ? ' ' : utf8::codepoint<Utf8Policy>(cursor_it));
        set_attribute(A_REVERSE, 0);
        if (cursor_it != end)
            addutf8str(utf8::next(cursor_it), end);
    }
    redraw(m_menu_win);
}

void NCursesUI::menu_show(const memoryview<String>& choices,
                          const DisplayCoord& anchor, MenuStyle style)
{
    assert(m_menu == nullptr);
    assert(m_menu_win == nullptr);
    m_choices = std::vector<String>(choices.begin(), choices.end());
    CharCount longest = 0;
    for (int i = 0; i < m_choices.size(); ++i)
    {
        m_items.push_back(new_item(m_choices[i].c_str(), ""));
        longest = std::max(longest, m_choices[i].char_length());
    }
    m_items.push_back(nullptr);
    longest += 1;

    int max_x,max_y;
    getmaxyx(stdscr, max_y, max_x);
    max_x -= (int)anchor.column;

    int columns = (style == MenuStyle::Prompt) ?
                  (max_x / std::min(max_x, (int)longest)) : 1;
    int lines = std::min(10, (int)ceilf((float)m_choices.size()/columns));

    m_menu_pos = { anchor.line+1, anchor.column };
    if (m_menu_pos.line + lines >= max_y)
        m_menu_pos.line = anchor.line - lines;
    m_menu_size = { lines, columns == 1 ? longest : max_x };

    m_menu = new_menu(&m_items[0]);
    m_menu_win = newwin((int)m_menu_size.line, (int)m_menu_size.column,
                        (int)m_menu_pos.line,  (int)m_menu_pos.column);
    set_menu_win(m_menu, m_menu_win);
    set_menu_format(m_menu, lines, columns);
    set_menu_mark(m_menu, nullptr);
    set_menu_fore(m_menu, COLOR_PAIR(m_menu_fg));
    set_menu_back(m_menu, COLOR_PAIR(m_menu_bg));
    post_menu(m_menu);
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
}

DisplayCoord NCursesUI::dimensions()
{
    int max_x,max_y;
    getmaxyx(stdscr, max_y, max_x);
    return {max_y - 1, max_x};
}

}
