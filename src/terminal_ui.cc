#include "terminal_ui.hh"

#include "display_buffer.hh"
#include "event_manager.hh"
#include "exception.hh"
#include "file.hh"
#include "keys.hh"
#include "ranges.hh"
#include "string_utils.hh"
#include "diff.hh"

#include <algorithm>

#include <fcntl.h>
#include <csignal>
#include <sys/ioctl.h>
#include <unistd.h>

constexpr char control(char c) { return c & 037; }

namespace Kakoune
{

using std::min;
using std::max;

static void set_cursor_pos(DisplayCoord coord)
{
    printf("\033[%d;%dH", (int)coord.line + 1, (int)coord.column + 1);
}

void TerminalUI::Window::create(const DisplayCoord& p, const DisplayCoord& s)
{
    pos = p;
    size = s;
    lines.resize((int)size.line);
}

void TerminalUI::Window::destroy()
{
    pos = DisplayCoord{};
    size = DisplayCoord{};
    lines.clear();
}

struct TerminalUI::Window::Line
{
    struct Atom
    {
        String text;
        ColumnCount skip = 0;
        Face face;

        ColumnCount length() const { return text.column_length() + skip; }
        void resize(ColumnCount size)
        {
            auto text_len = text.column_length();
            if (size < text_len)
            {
                text.resize(text.byte_count_to(size), 0);
                skip = 0;
            }
            else
                skip = size - text_len; 
        }

        friend bool operator==(const Atom& lhs, const Atom& rhs) { return lhs.text == rhs.text and lhs.skip == rhs.skip and lhs.face == rhs.face; }
        friend bool operator!=(const Atom& lhs, const Atom& rhs) { return not (lhs == rhs); }
        friend size_t hash_value(const Atom& atom) { return hash_values(atom.text, atom.skip, atom.face); }
    };

    void append(String text, ColumnCount skip, Face face)
    {
        if (not atoms.empty() and atoms.back().face == face and (atoms.back().skip == 0 or text.empty()))
        {
            atoms.back().text += text;
            atoms.back().skip += skip;
        }
        else
            atoms.push_back({std::move(text), skip, face});
    }

    void resize(ColumnCount width)
    {
        auto it = atoms.begin();
        ColumnCount column = 0;
        for (; it != atoms.end() and column < width; ++it)
            column += it->length();

        if (column < width)
            append({}, width - column, atoms.empty() ? Face{} : atoms.back().face);
        else
        {
            atoms.erase(it, atoms.end());
            if (column > width)
                atoms.back().resize(atoms.back().length() - (column - width));
        }
    }

    Vector<Atom>::iterator erase_range(ColumnCount pos, ColumnCount len)
    {
        struct Pos{ Vector<Atom>::iterator it; ColumnCount column; };
        auto find_col = [pos=0_col, it=atoms.begin(), end=atoms.end()](ColumnCount col) mutable {
            for (; it != end; ++it)
            {
                auto atom_len = it->length();
                if (pos + atom_len >= col)
                    return Pos{it, col - pos};
                pos += atom_len;
            }
            return Pos{it, 0_col};
        };
        Pos begin = find_col(pos);
        Pos end = find_col(pos+len);

        auto make_tail = [](const Atom& atom, ColumnCount from) {
            if (atom.text.column_length() > from)
                return Atom{atom.text.substr(atom.text.byte_count_to(from)).str(), atom.skip, atom.face};
            else
                return Atom{{}, atom.length() - from, atom.face};
        };

        if (begin.it == end.it)
        {
            Atom tail = make_tail(*begin.it, end.column);
            begin.it->resize(begin.column);
            return (tail.text.empty() and tail.skip == 0) ? begin.it+1 : atoms.insert(begin.it+1, tail);
        }

        begin.it->resize(begin.column);
        if (end.column > 0)
        {
            if (end.column == end.it->length())
                ++end.it;
            else
                *end.it = make_tail(*end.it, end.column);
        }
        return atoms.erase(begin.it+1, end.it);
    }

    Vector<Atom> atoms;
};

void TerminalUI::Window::blit(Window& target)
{
    kak_assert(pos.line + lines.size() <= target.lines.size());
    auto target_line = target.lines.begin() + (size_t)pos.line;
    for (auto& line : lines)
    {
        line.resize(size.column);
        target_line->resize(target.size.column);
        target_line->atoms.insert(target_line->erase_range(pos.column, size.column),
                                  line.atoms.begin(), line.atoms.end());
        ++target_line;
    }
}

void TerminalUI::Window::draw(DisplayCoord pos,
                              ConstArrayView<DisplayAtom> atoms,
                             const Face& default_face)
{
    lines[(size_t)pos.line].resize(pos.column);
    for (const DisplayAtom& atom : atoms)
    {
        StringView content = atom.content();
        if (content.empty())
            continue;

        auto face = merge_faces(default_face, atom.face);
        if (content.back() == '\n')
            lines[(int)pos.line].append(content.substr(0, content.length()-1).str(), 1, face);
        else
            lines[(int)pos.line].append(content.str(), 0, face);
        pos.column += content.column_length();
    }

    if (pos.column < size.column)
        lines[(int)pos.line].append({}, size.column - pos.column, default_face);
}

void TerminalUI::Screen::set_face(const Face& face)
{
    static constexpr int fg_table[]{ 39, 30, 31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97 };
    static constexpr int bg_table[]{ 49, 40, 41, 42, 43, 44, 45, 46, 47, 100, 101, 102, 103, 104, 105, 106, 107 };
    static constexpr int attr_table[]{ 0, 4, 7, 5, 1, 2, 3 };

    auto set_color = [](bool fg, const Color& color, bool join) {
        if (join)
            fputs(";", stdout);
        if (color.isRGB())
            printf("%d;2;%d;%d;%d", fg ? 38 : 48, color.r, color.g, color.b);
        else
            printf("%d", (fg ? fg_table : bg_table)[(int)(char)color.color]);
    };

    if (m_active_face == face)
        return;

    fputs("\033[", stdout);
    bool join = false;
    if (face.attributes != m_active_face.attributes)
    {
        for (int i = 0; i < sizeof(attr_table) / sizeof(int); ++i)
        {
            if (face.attributes & (Attribute)(1 << i))
                printf(";%d", attr_table[i]);
        }
        m_active_face = Face{{}, {}, face.attributes};
        join = true;
    }
    if (m_active_face.fg != face.fg)
    {
        set_color(true, face.fg, join);
        join = true;
    }
    if (m_active_face.bg != face.bg)
        set_color(false, face.bg, join);
    fputs("m", stdout);

    m_active_face = face;
}

void TerminalUI::Screen::output(bool force)
{
    if (lines.empty())
        return;

    struct Change { int keep; int add; int del; };
    Vector<Change> changes{Change{}};
    auto new_hashes = lines | transform([](auto& line) { return hash_value(line.atoms); }) | gather<Vector>();
    for_each_diff(hashes.begin(), hashes.size(),
                  new_hashes.begin(), new_hashes.size(),
                  [&changes](DiffOp op, int len) mutable {
        switch (op)
        {
            case DiffOp::Keep:
                changes.push_back({len, 0, 0});
                break;
            case DiffOp::Add:
                changes.back().add += len;
                break;
            case DiffOp::Remove:
                changes.back().del += len;
                break;
        }
    });
    hashes = std::move(new_hashes);

    int line = 0;
    for (auto& change : changes)
    {
        line += change.keep;
        if (int del = std::max(change.del - change.add, 0); del > 0)
        {
            printf("\033[%dH\033[%dM", line + 1, del);
            line -= del;
        }
        line += change.del;
    }

    line = 0;
    for (auto& change : changes)
    {
        line += change.keep;
        for (int i = 0; i < change.add; ++i)
        {
            if (int add = std::max(0, change.add - change.del); i == 0 and add > 0)
                printf("\033[%dH\033[%dL", line + 1, add);
            else
                printf("\033[%dH", line + 1);

            ColumnCount pending_move = 0;
            for (auto& [text, skip, face] : lines[line++].atoms)
            {
                if (text.empty() and skip == 0)
                    continue;

                if (pending_move != 0)
                {
                    printf("\033[%dC", (int)pending_move);
                    pending_move = 0;
                }
                set_face(face);
                fputs(text.c_str(), stdout);
                if (skip > 3 and face.attributes == Attribute{})
                {
                    fputs("\033[K", stdout);
                    pending_move = skip;
                }
                else if (skip > 0)
                    fputs(String{' ', skip}.c_str(), stdout);
            }
        }
    }
}

constexpr int TerminalUI::default_shift_function_key;

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

static constexpr StringView assistant_dilbert[] =
    { R"(  დოოოოოდ   )",
      R"(  |     |   )",
      R"(  |     |  ╭)",
      R"(  |-ᱛ ᱛ-|  │)",
      R"( Ͼ   ∪   Ͽ │)",
      R"(  |     |  ╯)",
      R"( ˏ`-.ŏ.-´ˎ  )",
      R"(     @      )",
      R"(      @     )",
      R"(            )"};

template<typename T> T sq(T x) { return x * x; }

static sig_atomic_t resize_pending = 0;
static sig_atomic_t sighup_raised = 0;

template<sig_atomic_t* signal_flag>
static void signal_handler(int)
{
    *signal_flag = 1;
    EventManager::instance().force_signal(0);
}

TerminalUI::TerminalUI()
    : m_cursor{CursorMode::Buffer, {}},
      m_stdin_watcher{STDIN_FILENO, FdEvents::Read,
                      [this](FDWatcher&, FdEvents, EventMode) {
        if (not m_on_key)
            return;

        while (auto key = get_next_key())
            m_on_key(*key);
      }},
      m_assistant(assistant_clippy)
{
    if (not isatty(1))
        throw runtime_error("stdout is not a tty");

    tcgetattr(STDIN_FILENO, &m_original_termios);

    setup_terminal();
    set_raw_mode();
    enable_mouse(true);

    set_signal_handler(SIGWINCH, &signal_handler<&resize_pending>);
    set_signal_handler(SIGHUP, &signal_handler<&sighup_raised>);
    set_signal_handler(SIGTSTP, [](int){ TerminalUI::instance().suspend(); });

    check_resize(true);
    redraw(false);
}

TerminalUI::~TerminalUI()
{
    enable_mouse(false);
    restore_terminal();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_original_termios);
    set_signal_handler(SIGWINCH, SIG_DFL);
    set_signal_handler(SIGHUP, SIG_DFL);
    set_signal_handler(SIGTSTP, SIG_DFL);
}

void TerminalUI::suspend()
{
    bool mouse_enabled = m_mouse_enabled;
    enable_mouse(false);
    restore_terminal();

    auto current = set_signal_handler(SIGTSTP, SIG_DFL);
    sigset_t unblock_sigtstp, old_mask;
    sigemptyset(&unblock_sigtstp);
    sigaddset(&unblock_sigtstp, SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &unblock_sigtstp, &old_mask);

    raise(SIGTSTP); // suspend here

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_original_termios);
    set_signal_handler(SIGTSTP, current);
    sigprocmask(SIG_SETMASK, &old_mask, nullptr);

    setup_terminal();
    check_resize(true);
    set_raw_mode();
    enable_mouse(mouse_enabled);

    refresh(true);
}

void TerminalUI::set_raw_mode() const
{
    termios attr = m_original_termios;
    attr.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    attr.c_oflag &= ~OPOST;
    attr.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    attr.c_lflag |= NOFLSH;
    attr.c_cflag &= ~(CSIZE | PARENB);
    attr.c_cflag |= CS8;
    attr.c_cc[VMIN] = attr.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr);
}

void TerminalUI::redraw(bool force)
{
    m_window.blit(m_screen);

    if (m_menu.columns != 0 or m_menu.pos.column > m_status_len)
        m_menu.blit(m_screen);

    m_info.blit(m_screen);

    m_screen.output(force);
    if (m_cursor.mode == CursorMode::Prompt)
        set_cursor_pos({m_status_on_top ? 0 : m_dimensions.line, m_cursor.coord.column});
    else
        set_cursor_pos(m_cursor.coord + content_line_offset());

    fflush(stdout);
}

void TerminalUI::set_cursor(CursorMode mode, DisplayCoord coord)
{
    m_cursor = Cursor{mode, coord};
}

void TerminalUI::refresh(bool force)
{
    if (m_dirty or force)
        redraw(force);
    m_dirty = false;
}

static const DisplayLine empty_line = { String(" "), {} };

void TerminalUI::draw(const DisplayBuffer& display_buffer,
                     const Face& default_face,
                     const Face& padding_face)
{
    check_resize();

    const DisplayCoord dim = dimensions();
    const LineCount line_offset = content_line_offset();
    LineCount line_index = line_offset;
    for (const DisplayLine& line : display_buffer.lines())
        m_window.draw(line_index++, line.atoms(), default_face);

    auto face = merge_faces(default_face, padding_face);
    while (line_index < dim.line + line_offset)
        m_window.draw(line_index++, DisplayAtom("~"), face);

    m_dirty = true;
}

void TerminalUI::draw_status(const DisplayLine& status_line,
                            const DisplayLine& mode_line,
                            const Face& default_face)
{
    const LineCount status_line_pos = m_status_on_top ? 0 : m_dimensions.line;
    m_window.draw(status_line_pos, status_line.atoms(), default_face);

    const auto mode_len = mode_line.length();
    m_status_len = status_line.length();
    const auto remaining = m_dimensions.column - m_status_len;
    if (mode_len < remaining)
    {
        ColumnCount col = m_dimensions.column - mode_len;
        m_window.draw({status_line_pos, col}, mode_line.atoms(), default_face);
    }
    else if (remaining > 2)
    {
        DisplayLine trimmed_mode_line = mode_line;
        trimmed_mode_line.trim(mode_len + 2 - remaining, remaining - 2);
        trimmed_mode_line.insert(trimmed_mode_line.begin(), { "…", {} });
        kak_assert(trimmed_mode_line.length() == remaining - 1);

        ColumnCount col = m_dimensions.column - remaining + 1;
        m_window.draw({status_line_pos, col}, trimmed_mode_line.atoms(), default_face);
    }

    if (m_set_title)
    {
        constexpr char suffix[] = " - Kakoune\007";
        char buf[4 + 511 + 2] = "\033]2;";
        // Fill title escape sequence buffer, removing non ascii characters
        auto buf_it = &buf[4], buf_end = &buf[4 + 511 - (sizeof(suffix) - 2)];
        for (auto& atom : mode_line)
        {
            const auto str = atom.content();
            for (auto it = str.begin(), end = str.end();
                 it != end and buf_it != buf_end; utf8::to_next(it, end))
                *buf_it++ = (*it >= 0x20 and *it <= 0x7e) ? *it : '?';
        }
        for (auto c : suffix)
            *buf_it++ = c;

        fputs(buf, stdout);
        fflush(stdout);
    }

    m_dirty = true;
}

void TerminalUI::check_resize(bool force)
{
    if (not force and not resize_pending)
        return;

    resize_pending = 0;

    const int fd = open("/dev/tty", O_RDWR);
    if (fd < 0)
        return;
    auto close_fd = on_scope_end([fd]{ ::close(fd); });

    winsize ws;
    if (::ioctl(fd, TIOCGWINSZ, &ws) != 0)
        return;

    const bool info = (bool)m_info;
    const bool menu = (bool)m_menu;
    if (m_window) m_window.destroy();
    if (info) m_info.destroy();
    if (menu) m_menu.destroy();

    m_window.create({0, 0}, {ws.ws_row, ws.ws_col});
    m_screen.create({0, 0}, {ws.ws_row, ws.ws_col});
    m_screen.hashes.clear();
    kak_assert(m_window);

    m_dimensions = DisplayCoord{ws.ws_row-1, ws.ws_col};

    // if (char* csr = tigetstr((char*)"csr"))
    //     putp(tparm(csr, 0, ws.ws_row));

    if (menu)
        menu_show(Vector<DisplayLine>(std::move(m_menu.items)),
                  m_menu.anchor, m_menu.fg, m_menu.bg, m_menu.style);
    if (info)
        info_show(m_info.title, m_info.content, m_info.anchor, m_info.face, m_info.style);

    set_resize_pending();
}

Optional<Key> TerminalUI::get_next_key()
{
    if (sighup_raised)
    {
        set_signal_handler(SIGWINCH, SIG_DFL);
        set_signal_handler(SIGHUP, SIG_DFL);
        if (m_window)
            m_window.destroy();
        m_stdin_watcher.disable();
        return {};
    }

    check_resize();

    if (m_resize_pending)
    {
        m_resize_pending = false;
        return resize(dimensions());
    }

    static auto get_char = []() -> Optional<unsigned char> {
        unsigned char c = 0;
        if (fd_readable(STDIN_FILENO) and read(STDIN_FILENO, &c, 1) == 1)
            return c;
        return {};
    };

    const auto c = get_char();
    if (not c)
        return {};

    static constexpr auto convert = [](Codepoint c) -> Codepoint {
        if (c == control('m') or c == control('j'))
            return Key::Return;
        if (c == control('i'))
            return Key::Tab;
        if (c == control('h') or c == 127)
            return Key::Backspace;
        return c;
    };
    auto parse_key = [](unsigned char c) -> Key {
        if (Codepoint cp = convert(c); cp > 255)
            return Key{cp};
        if (c == control('z'))
        {
            kill(0, SIGTSTP); // We suspend at this line
            return {};
        }
        if (c < 27)
            return ctrl(c - 1 + 'a');

       struct Sentinel{};
       struct CharIterator
       {
           unsigned char operator*() { if (not c) c = get_char().value_or((unsigned char)0); return *c; }
           CharIterator& operator++() { c.reset(); return *this; }
           bool operator==(const Sentinel&) const { return false; }
           Optional<unsigned char> c;
       };
       return Key{utf8::codepoint(CharIterator{c}, Sentinel{})};
    };

    auto parse_csi = [this]() -> Optional<Key> {
        auto next_char = [] { return get_char().value_or((unsigned char)0xff); };
        int params[16] = {};
        auto c = next_char();
        char private_mode = 0;
        if (c == '?' or c == '<' or c == '=' or c == '>')
        {
            private_mode = c;
            c = next_char();
        }
        for (int count = 0; count < 16 and c >= 0x30 && c <= 0x3f; c = next_char())
        {
            if (isdigit(c))
                params[count] = params[count] * 10 + c - '0';
            else if (c == ';')
                ++count;
            else
                return {};
        }
        if (c != '$' and (c < 0x40 or c > 0x7e))
            return {};

        auto parse_mask = [](int mask) {
            Key::Modifiers mod = Key::Modifiers::None;
            if (mask & 1)
                mod |= Key::Modifiers::Shift;
            if (mask & 2)
                mod |= Key::Modifiers::Alt;
            if (mask & 4)
                mod |= Key::Modifiers::Control;
            return mod;
        };

        auto mouse_button = [this](Key::Modifiers mod, Codepoint coord, bool left, bool release) {
            auto mask = left ? 0x1 : 0x2;
            if (not release)
            {
                mod |= (m_mouse_state & mask) ? Key::Modifiers::MousePos : (left ? Key::Modifiers::MousePressLeft : Key::Modifiers::MousePressRight);
                m_mouse_state |= mask;
            }
            else
            {
                mod |= left ? Key::Modifiers::MouseReleaseLeft : Key::Modifiers::MouseReleaseRight;
                m_mouse_state &= ~mask;
            }
            return Key{mod, coord};
        };

        auto mouse_scroll = [this](Key::Modifiers mod, bool down) -> Key {
            return {mod | Key::Modifiers::Scroll,
                    (Codepoint)((down ? 1 : -1) * m_wheel_scroll_amount)};
        };

        auto masked_key = [&](Codepoint key) { return Key{parse_mask(std::max(params[1] - 1, 0)), key}; };

        switch (c)
        {
        case '$':
            switch (params[0])
            {
            case 23: case 24:
                return Key{Key::Modifiers::Shift, Key::F11 + params[0] - 23}; // rxvt style
            }
            return {};
        case 'A': return masked_key(Key::Up);
        case 'B': return masked_key(Key::Down);
        case 'C': return masked_key(Key::Right);
        case 'D': return masked_key(Key::Left);
        case 'F': return masked_key(Key::End);   // PC/xterm style
        case 'H': return masked_key(Key::Home);  // PC/xterm style
        case 'P': return masked_key(Key::F1);
        case 'Q': return masked_key(Key::F2);
        case 'R': return masked_key(Key::F3);
        case 'S': return masked_key(Key::F4);
        case '~':
            switch (params[0])
            {
            case 1: return masked_key(Key::Home);     // VT220/tmux style
            case 2: return masked_key(Key::Insert);
            case 3: return masked_key(Key::Delete);
            case 4: return masked_key(Key::End);      // VT220/tmux style
            case 5: return masked_key(Key::PageUp);
            case 6: return masked_key(Key::PageDown);
            case 7: return masked_key(Key::Home);     // rxvt style
            case 8: return masked_key(Key::End);      // rxvt style
            case 11: case 12: case 13: case 14: case 15:
                return masked_key(Key::F1 + params[0] - 11);
            case 17: case 18: case 19: case 20: case 21:
                return masked_key(Key::F6 + params[0] - 17);
            case 23: case 24:
                return masked_key(Key::F11 + params[0] - 23);
            case 25: case 26:
                return Key{Key::Modifiers::Shift, Key::F3 + params[0] - 25}; // rxvt style
            case 28: case 29:
                return Key{Key::Modifiers::Shift, Key::F5 + params[0] - 28}; // rxvt style
            case 31: case 32:
                return Key{Key::Modifiers::Shift, Key::F7 + params[0] - 31}; // rxvt style
            case 33: case 34:
                return Key{Key::Modifiers::Shift, Key::F9 + params[0] - 33}; // rxvt style
            }
            return {};
        case 'u':
            return masked_key(convert(static_cast<Codepoint>(params[0])));
        case 'Z': return shift(Key::Tab);
        case 'I': return {Key::FocusIn};
        case 'O': return {Key::FocusOut};
        case 'M': case 'm':
            const bool sgr = private_mode == '<';
            if (not sgr and c != 'M')
                return {};

            const Codepoint b = sgr ? params[0] : next_char() - 32;
            const int x = (sgr ? params[1] : next_char() - 32) - 1;
            const int y = (sgr ? params[2] : next_char() - 32) - 1;
            auto coord = encode_coord({y - content_line_offset(), x});
            Key::Modifiers mod = parse_mask((b >> 2) & 0x7);
            switch (b & 0x43)
            {
            case 0: return mouse_button(mod, coord, true, c == 'm');
            case 2: return mouse_button(mod, coord, false, c == 'm');
            case 3:
                if (sgr)
                    return {};
                if (m_mouse_state & 0x1)
                    return mouse_button(mod, coord, true, true);
                else if (m_mouse_state & 0x2)
                    return mouse_button(mod, coord, false, true);
                break;
            case 64: return mouse_scroll(mod, false);
            case 65: return mouse_scroll(mod, true);
            }
            return Key{Key::Modifiers::MousePos, coord};
        }
        return {};
    };

    auto parse_ss3 = []() -> Optional<Key> {
        switch (get_char().value_or((unsigned char)0xff))
        {
        case 'A': return Key{Key::Up};
        case 'B': return Key{Key::Down};
        case 'C': return Key{Key::Right};
        case 'D': return Key{Key::Left};
        case 'F': return Key{Key::End};
        case 'H': return Key{Key::Home};
        case 'P': return Key{Key::F1};
        case 'Q': return Key{Key::F2};
        case 'R': return Key{Key::F3};
        case 'S': return Key{Key::F4};
        default: return {};
        }
    };

    if (*c != 27)
        return parse_key(*c);

    if (auto next = get_char())
    {
        if (*next == '[') // potential CSI
            return parse_csi().value_or(alt('['));
        if (*next == 'O') // potential SS3
            return parse_ss3().value_or(alt('O'));
        return alt(parse_key(*next));
    }
    return Key{Key::Escape};
}

template<typename T>
T div_round_up(T a, T b)
{
    return (a - T(1)) / b + T(1);
}

void TerminalUI::draw_menu()
{
    // menu show may have not created the window if it did not fit.
    // so be tolerant.
    if (not m_menu)
        return;

    const int item_count = (int)m_menu.items.size();
    if (m_menu.columns == 0)
    {
        const auto win_width = m_menu.size.column - 4;
        kak_assert(m_menu.size.line == 1);
        ColumnCount pos = 0;

        m_menu.draw({0, 0}, DisplayAtom(m_menu.first_item > 0 ? "< " : ""), m_menu.bg);

        int i = m_menu.first_item;
        for (; i < item_count and pos < win_width; ++i)
        {
            const DisplayLine& item = m_menu.items[i];
            const ColumnCount item_width = item.length();
            auto& face = i == m_menu.selected_item ? m_menu.fg : m_menu.bg;
            m_menu.draw({0, pos+2}, item.atoms(), face);
            if (pos + item_width >= win_width)
                m_menu.draw({0, win_width+2}, DisplayAtom("…"), m_menu.bg);
            pos += item_width + 1;
        }

        if (i != item_count)
            m_menu.draw({0, win_width+3}, DisplayAtom(">"), m_menu.bg);

        m_dirty = true;
        return;
    }

    const LineCount menu_lines = div_round_up(item_count, m_menu.columns);
    const LineCount win_height = m_menu.size.line;
    kak_assert(win_height <= menu_lines);

    const ColumnCount column_width = (m_menu.size.column - 1) / m_menu.columns;

    const LineCount mark_height = min(div_round_up(sq(win_height), menu_lines),
                                      win_height);

    const int menu_cols = div_round_up(item_count, (int)m_menu.size.line);
    const int first_col = m_menu.first_item / (int)m_menu.size.line;

    const LineCount mark_line = (win_height - mark_height) * first_col / max(1, menu_cols - m_menu.columns);

    for (auto line = 0_line; line < win_height; ++line)
    {
        for (int col = 0; col < m_menu.columns; ++col)
        {
            int item_idx = (first_col + col) * (int)m_menu.size.line + (int)line;
            auto& face = item_idx < item_count and item_idx == m_menu.selected_item ? m_menu.fg : m_menu.bg;
            auto atoms = item_idx < item_count ? m_menu.items[item_idx].atoms() : ConstArrayView<DisplayAtom>{};
            m_menu.draw({line, col * column_width}, atoms, face);
        }
        const bool is_mark = line >= mark_line and line < mark_line + mark_height;
        m_menu.draw({line, m_menu.size.column - 1}, DisplayAtom(is_mark ? "█" : "░"), m_menu.bg);
    }
    m_dirty = true;
}

static LineCount height_limit(MenuStyle style)
{
    switch (style)
    {
        case MenuStyle::Inline: return 10_line;
        case MenuStyle::Prompt: return 10_line;
        case MenuStyle::Search: return 3_line;
    }
    kak_assert(false);
    return 0_line;
}

void TerminalUI::menu_show(ConstArrayView<DisplayLine> items,
                          DisplayCoord anchor, Face fg, Face bg,
                          MenuStyle style)
{
    if (m_menu)
    {
        m_menu.destroy();
        m_dirty = true;
    }

    m_menu.fg = fg;
    m_menu.bg = bg;
    m_menu.style = style;
    m_menu.anchor = anchor;

    if (m_dimensions.column <= 2)
        return;

    const int item_count = items.size();
    m_menu.items.clear(); // make sure it is empty
    m_menu.items.reserve(item_count);
    const auto longest = accumulate(items | transform(&DisplayLine::length),
                                    1_col, [](auto&& lhs, auto&& rhs) { return std::max(lhs, rhs); });

    const ColumnCount max_width = m_dimensions.column - 1;
    const bool is_inline = style == MenuStyle::Inline;
    const bool is_search = style == MenuStyle::Search;
    m_menu.columns = is_search ? 0 : (is_inline ? 1 : max((int)(max_width / (longest+1)), 1));

    const LineCount max_height = min(height_limit(style), max(anchor.line, m_dimensions.line - anchor.line - 1));
    const LineCount height = is_search ?
        1 : (min<LineCount>(max_height, div_round_up(item_count, m_menu.columns)));

    const ColumnCount maxlen = (m_menu.columns > 1 and item_count > 1) ?
        max_width / m_menu.columns - 1 : max_width;

    for (auto& item : items)
    {
        m_menu.items.push_back(item);
        m_menu.items.back().trim(0, maxlen);
        kak_assert(m_menu.items.back().length() <= maxlen);
    }

    if (is_inline)
        anchor.line += content_line_offset();

    LineCount line = anchor.line + 1;
    ColumnCount column = std::max(0_col, std::min(anchor.column, m_dimensions.column - longest - 1));
    if (is_search)
    {
        line = m_status_on_top ? 0_line : m_dimensions.line;
        column = m_dimensions.column / 2;
    }
    else if (not is_inline)
        line = m_status_on_top ? 1_line : m_dimensions.line - height;
    else if (line + height > m_dimensions.line)
        line = anchor.line - height;

    const auto width = is_search ? m_dimensions.column - m_dimensions.column / 2
                                 : (is_inline ? min(longest+1, m_dimensions.column)
                                              : m_dimensions.column);
    m_menu.create({line, column}, {height, width});
    m_menu.selected_item = item_count;
    m_menu.first_item = 0;

    draw_menu();

    if (m_info)
        info_show(m_info.title, m_info.content,
                  m_info.anchor, m_info.face, m_info.style);
}

void TerminalUI::menu_select(int selected)
{
    const int item_count = m_menu.items.size();
    if (selected < 0 or selected >= item_count)
    {
        m_menu.selected_item = -1;
        m_menu.first_item = 0;
    }
    else if (m_menu.columns == 0) // Do not columnize
    {
        m_menu.selected_item = selected;
        const ColumnCount width = m_menu.size.column - 3;
        int first = 0;
        ColumnCount item_col = 0;
        for (int i = 0; i <= selected; ++i)
        {
            const ColumnCount item_width = m_menu.items[i].length() + 1;
            if (item_col + item_width > width)
            {
                first = i;
                item_col = item_width;
            }
            else
                item_col += item_width;
        }
        m_menu.first_item = first;
    }
    else
    {
        m_menu.selected_item = selected;
        const int menu_cols = div_round_up(item_count, (int)m_menu.size.line);
        const int first_col = m_menu.first_item / (int)m_menu.size.line;
        const int selected_col = m_menu.selected_item / (int)m_menu.size.line;
        if (selected_col < first_col)
            m_menu.first_item = selected_col * (int)m_menu.size.line;
        if (selected_col >= first_col + m_menu.columns)
            m_menu.first_item = min(selected_col, menu_cols - m_menu.columns) * (int)m_menu.size.line;
    }
    draw_menu();
}

void TerminalUI::menu_hide()
{
    if (not m_menu)
        return;

    m_menu.items.clear();
    m_menu.destroy();
    m_dirty = true;

    // Recompute info as it does not have to avoid the menu anymore
    if (m_info)
        info_show(m_info.title, m_info.content, m_info.anchor, m_info.face, m_info.style);
}

static DisplayCoord compute_pos(DisplayCoord anchor, DisplayCoord size,
                                TerminalUI::Rect rect, TerminalUI::Rect to_avoid,
                                bool prefer_above)
{
    DisplayCoord pos;
    if (prefer_above)
    {
        pos = anchor - DisplayCoord{size.line};
        if (pos.line < 0)
            prefer_above = false;
    }
    auto rect_end = rect.pos + rect.size;
    if (not prefer_above)
    {
        pos = anchor + DisplayCoord{1_line};
        if (pos.line + size.line >= rect_end.line)
            pos.line = max(rect.pos.line, anchor.line - size.line);
    }
    if (pos.column + size.column >= rect_end.column)
        pos.column = max(rect.pos.column, rect_end.column - size.column);

    if (to_avoid.size != DisplayCoord{})
    {
        DisplayCoord to_avoid_end = to_avoid.pos + to_avoid.size;

        DisplayCoord end = pos + size;

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

static DisplayLineList wrap_lines(const DisplayLineList& lines, ColumnCount max_width)
{
    DisplayLineList result;
    for (auto line : lines)
    {
        ColumnCount column = 0;
        for (auto it = line.begin(); it != line.end(); )
        {
            auto length = it->length();
            column += length;
            if (column > max_width)
            {
                auto content = it->content().substr(0, length - (column - max_width));
                auto pos = find_if(content | reverse(), [](char c) { return not is_word(c); });
                if (pos != content.rend())
                    content = {content.begin(), pos.base()};

                if (not content.empty())
                    it = ++line.split(it, content.column_length());
                result.push_back(AtomList(std::make_move_iterator(line.begin()),
                                          std::make_move_iterator(it)));
                it = line.erase(line.begin(), it);
                column = 0;
            }
            else
                ++it;
        }
        result.push_back(std::move(line));
    }
    return result;
}

void TerminalUI::info_show(const DisplayLine& title, const DisplayLineList& content,
                          DisplayCoord anchor, Face face, InfoStyle style)
{
    info_hide();

    m_info.title = title;
    m_info.content = content;
    m_info.anchor = anchor;
    m_info.face = face;
    m_info.style = style;

    const bool framed = style == InfoStyle::Prompt or style == InfoStyle::Modal;
    const bool assisted = style == InfoStyle::Prompt and m_assistant.size() != 0;

    DisplayCoord max_size = m_dimensions;
    if (style == InfoStyle::MenuDoc)
        max_size.column = std::max(m_dimensions.column - (m_menu.pos.column + m_menu.size.column),
                                   m_menu.pos.column);
    else if (style != InfoStyle::Modal)
        max_size.line -= m_menu.size.line;

    const auto max_content_width = max_size.column - (framed ? 4 : 2) - (assisted ? m_assistant[0].column_length() : 0);
    if (max_content_width <= 0)
        return;

    auto compute_size = [](const DisplayLineList& lines) -> DisplayCoord {
        return {(int)lines.size(), accumulate(lines, 0_col, [](ColumnCount c, const DisplayLine& l) { return std::max(c, l.length()); })};
    };

    DisplayCoord content_size = compute_size(content);
    const bool wrap = content_size.column > max_content_width;
    DisplayLineList wrapped_content;
    if (wrap)
    {
        wrapped_content = wrap_lines(content, max_content_width);
        content_size = compute_size(wrapped_content);
    }
    const auto& lines = wrap ? wrapped_content : content;

    DisplayCoord size{content_size.line, std::max(content_size.column, title.length() + (framed ? 2 : 0))};
    if (framed)
        size += {2, 4};
    if (assisted)
        size = {std::max(LineCount{(int)m_assistant.size()-1}, size.line), size.column + m_assistant[0].column_length()};
    size = {std::min(max_size.line, size.line), std::min(max_size.column, size.column)};

    if ((framed and size.line < 3) or size.line <= 0)
        return;

    const Rect rect = {content_line_offset(), m_dimensions};
    if (style == InfoStyle::Prompt)
    {
        anchor = DisplayCoord{m_status_on_top ? 0 : m_dimensions.line, m_dimensions.column-1};
        anchor = compute_pos(anchor, size, rect, m_menu, style == InfoStyle::InlineAbove);
    }
    else if (style == InfoStyle::Modal)
    {
        auto half = [](const DisplayCoord& c) { return DisplayCoord{c.line / 2, c.column / 2}; };
        anchor = rect.pos + half(rect.size) - half(size);
    }
    else if (style == InfoStyle::MenuDoc)
    {
        const auto right_max_width = m_dimensions.column - (m_menu.pos.column + m_menu.size.column);
        const auto left_max_width = m_menu.pos.column;
        anchor.line = m_menu.pos.line;
        if (size.column <= right_max_width or right_max_width >= left_max_width)
            anchor.column = m_menu.pos.column + m_menu.size.column;
        else
            anchor.column = m_menu.pos.column - size.column;
    }
    else
    {
        anchor = compute_pos(anchor, size, rect, m_menu, style == InfoStyle::InlineAbove);
        anchor.line += content_line_offset();
    }

    m_info.create(anchor, size);
    for (auto line = 0_line; line < size.line; ++line)
    {
        auto draw_atoms = [&, this, pos=DisplayCoord{line}](auto&&... args) mutable {
            auto draw = overload(
                [&](ColumnCount padding) {
                    pos.column += padding;
                },
                [&](String str) {
                    auto len = str.column_length();
                    m_info.draw(pos, DisplayAtom{std::move(str)}, face);
                    pos.column += len;
                },
                [&](const DisplayLine& atoms) {
                    m_info.draw(pos, atoms.atoms(), face);
                    pos.column += atoms.length();
                });
            (draw(args), ...);
        };

        constexpr Codepoint dash{L'─'};
        constexpr Codepoint dotted_dash{L'┄'};
        if (assisted)
        {
            const auto assistant_top_margin = (size.line - m_assistant.size()+1) / 2;
            StringView assistant_line = (line >= assistant_top_margin) ?
                m_assistant[(int)min(line - assistant_top_margin, LineCount{(int)m_assistant.size()}-1)]
              : m_assistant[(int)m_assistant.size()-1];

            draw_atoms(assistant_line.str());
        }
        if (not framed)
            draw_atoms(lines[(int)line]);
        else if (line == 0)
        {
            if (title.atoms().empty() or content_size.column < 2)
                draw_atoms("╭─" + String{dash, content_size.column} + "─╮");
            else
            {
                auto trimmed_title = title;
                trimmed_title.trim(0, content_size.column - 2);
                auto dash_count = content_size.column - trimmed_title.length() - 2;
                String left{dash, dash_count / 2};
                String right{dash, dash_count - dash_count / 2};
                draw_atoms("╭─" + left + "┤", trimmed_title, "├" + right +"─╮");
            }
        }
        else if (line < size.line - 1 and line <= lines.size())
        {
            auto info_line = lines[(int)line - 1];
            const bool trimmed = info_line.trim(0, content_size.column);
            const ColumnCount padding = content_size.column - info_line.length();
            draw_atoms("│ ", info_line, padding, (trimmed ? "…│" : " │"));
        }
        else if (line == std::min<LineCount>((int)lines.size() + 1, size.line - 1))
            draw_atoms("╰─", String(line > lines.size() ? dash : dotted_dash, content_size.column), "─╯");
    }
    m_dirty = true;
}

void TerminalUI::info_hide()
{
    if (not m_info)
        return;
    m_info.destroy();
    m_dirty = true;
}

void TerminalUI::set_on_key(OnKeyCallback callback)
{
    m_on_key = std::move(callback);
    EventManager::instance().force_signal(0);
}

DisplayCoord TerminalUI::dimensions()
{
    return m_dimensions;
}

LineCount TerminalUI::content_line_offset() const
{
    return m_status_on_top ? 1 : 0;
}

void TerminalUI::set_resize_pending()
{
    m_resize_pending = true;
    EventManager::instance().force_signal(0);
}

void TerminalUI::setup_terminal()
{
    fputs("\033[?1049h", stdout);
    fputs("\033[?1004h", stdout);
    fputs("\033[?25l", stdout);
    fputs("\033=", stdout);
    fflush(stdout);
}

void TerminalUI::restore_terminal()
{
    fputs("\033[?1049l", stdout);
    fputs("\033[?1004l", stdout);
    fputs("\033[?25h", stdout);
    fputs("\033>", stdout);
    fputs("\033[m", stdout);
    fflush(stdout);
}

void TerminalUI::enable_mouse(bool enabled)
{
    if (enabled == m_mouse_enabled)
        return;

    m_mouse_enabled = enabled;
    if (enabled)
    {
        // force SGR mode
        fputs("\033[?1006h", stdout);
        // enable mouse
        fputs("\033[?1000h", stdout);
        // force enable report mouse position
        fputs("\033[?1002h", stdout);
    }
    else
    {
        fputs("\033[?1002l", stdout);
        fputs("\033[?1000l", stdout);
        fputs("\033[?1006l", stdout);
    }
    fflush(stdout);
}

void TerminalUI::set_ui_options(const Options& options)
{
    {
        auto it = options.find("terminal_assistant"_sv);
        if (it == options.end() or it->value == "clippy")
            m_assistant = assistant_clippy;
        else if (it->value == "cat")
            m_assistant = assistant_cat;
        else if (it->value == "dilbert")
            m_assistant = assistant_dilbert;
        else if (it->value == "none" or it->value == "off")
            m_assistant = ConstArrayView<StringView>{};
    }

    {
        auto it = options.find("terminal_status_on_top"_sv);
        m_status_on_top = it != options.end() and
            (it->value == "yes" or it->value == "true");
    }

    {
        auto it = options.find("terminal_set_title"_sv);
        m_set_title = it == options.end() or
            (it->value == "yes" or it->value == "true");
    }

    {
        auto it = options.find("terminal_shift_function_key"_sv);
        m_shift_function_key = it != options.end() ?
            str_to_int_ifp(it->value).value_or(default_shift_function_key)
          : default_shift_function_key;
    }

    {
        auto enable_mouse_it = options.find("terminal_enable_mouse"_sv);
        enable_mouse(enable_mouse_it == options.end() or
                     enable_mouse_it->value == "yes" or
                     enable_mouse_it->value == "true");

        auto wheel_up_it = options.find("terminal_wheel_up_button"_sv);
        m_wheel_up_button = wheel_up_it != options.end() ?
            str_to_int_ifp(wheel_up_it->value).value_or(4) : 4;

        auto wheel_down_it = options.find("terminal_wheel_down_button"_sv);
        m_wheel_down_button = wheel_down_it != options.end() ?
            str_to_int_ifp(wheel_down_it->value).value_or(5) : 5;

        auto wheel_scroll_amount_it = options.find("terminal_wheel_scroll_amount"_sv);
        m_wheel_scroll_amount = wheel_scroll_amount_it != options.end() ?
            str_to_int_ifp(wheel_scroll_amount_it->value).value_or(3) : 3;
    }
}

}
