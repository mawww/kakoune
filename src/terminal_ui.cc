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
#include <strings.h>

namespace Kakoune
{

using std::min;
using std::max;

static String fix_atom_text(StringView str)
{
    String res;
    auto pos = str.begin();
    for (auto it = str.begin(), end = str.end(); it != end; ++it)
    {
        char c = *it;
        if (c >= 0 and c <= 0x1F)
        {
            res += StringView{pos, it};
            res += String{Codepoint{(uint32_t)(0x2400 + c)}};
            pos = it+1;
        }
    }
    res += StringView{pos, str.end()};
    return res;
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
            auto it = text.begin(), end = text.end();
            while (it != end and size > 0)
                size -= codepoint_width(utf8::read_codepoint(it, end));

            if (size < 0) // possible if resizing to the middle of a double-width codepoint
            {
                kak_assert(size == -1);
                utf8::to_previous(it, text.begin());
                skip = 1;
            }
            else
                skip = size;
            text.resize(it - text.begin(), 0);
        }

        friend bool operator==(const Atom& lhs, const Atom& rhs) { return lhs.text == rhs.text and lhs.skip == rhs.skip and lhs.face == rhs.face; }
        friend bool operator!=(const Atom& lhs, const Atom& rhs) { return not (lhs == rhs); }
        friend size_t hash_value(const Atom& atom) { return hash_values(atom.text, atom.skip, atom.face); }
    };

    void append(StringView text, ColumnCount skip, Face face)
    {
        if (not atoms.empty() and atoms.back().face == face and (atoms.back().skip == 0 or text.empty()))
        {
            atoms.back().text += fix_atom_text(text);
            atoms.back().skip += skip;
        }
        else
            atoms.push_back({fix_atom_text(text), skip, face});
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
            auto it = atom.text.begin(), end = atom.text.end();
            while (it != end and from > 0)
                from -= codepoint_width(utf8::read_codepoint(it, end));

            if (from < 0) // can happen if tail starts in the middle of a double-width codepoint
            {
                kak_assert(from == -1);
                return Atom{" " + StringView{it, end}, atom.skip, atom.face};
            }
            return Atom{{it, end}, atom.skip - from, atom.face};
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

void TerminalUI::Window::create(const DisplayCoord& p, const DisplayCoord& s)
{
    kak_assert(p.line >= 0 and p.column >= 0);
    kak_assert(s.line >= 0 and s.column >= 0);
    pos = p;
    size = s;
    lines.reset(new Line[(int)size.line]);
}

void TerminalUI::Window::destroy()
{
    pos = DisplayCoord{};
    size = DisplayCoord{};
    lines.reset();
}

void TerminalUI::Window::blit(Window& target)
{
    kak_assert(pos.line < target.size.line);
    LineCount line_index = pos.line;
    for (auto& line : ArrayView{lines.get(), (size_t)size.line})
    {
        line.resize(size.column);
        auto& target_line = target.lines[(size_t)line_index];
        target_line.resize(target.size.column);
        target_line.atoms.insert(target_line.erase_range(pos.column, size.column), line.atoms.begin(), line.atoms.end());
        if (++line_index == target.size.line)
            break;
    }
}

void TerminalUI::Window::draw(DisplayCoord pos,
                              ConstArrayView<DisplayAtom> atoms,
                              const Face& default_face)
{
    if (pos.line >= size.line) // We might receive an out of date draw command after a resize
        return;

    lines[(size_t)pos.line].resize(pos.column);
    for (const DisplayAtom& atom : atoms)
    {
        StringView content = atom.content();
        if (content.empty())
            continue;

        auto face = merge_faces(default_face, atom.face);
        if (content.back() == '\n')
            lines[(int)pos.line].append(content.substr(0, content.length()-1), 1, face);
        else
            lines[(int)pos.line].append(content, 0, face);
        pos.column += content.column_length();
    }

    if (pos.column < size.column)
        lines[(int)pos.line].append({}, size.column - pos.column, default_face);
}

struct Writer : BufferedWriter<>
{
    using Writer::BufferedWriter::BufferedWriter;
    ~Writer() noexcept(false) = default;
};

template<typename... Args>
static void format_with(Writer& writer, StringView format, Args&&... args)
{
    format_with([&](StringView s) { writer.write(s); }, format, std::forward<Args>(args)...);
}

void TerminalUI::Screen::set_face(const Face& face, Writer& writer)
{
    static constexpr int fg_table[]{ 39, 30, 31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97 };
    static constexpr int bg_table[]{ 49, 40, 41, 42, 43, 44, 45, 46, 47, 100, 101, 102, 103, 104, 105, 106, 107 };
    static constexpr int ul_table[]{ 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    static constexpr const char* attr_table[]{ "0", "4", "4:3", "7", "5", "1", "2", "3", "9" };

    auto set_color = [&](bool fg, const Color& color, bool join) {
        if (join)
            writer.write(";");
        if (color.isRGB())
            format_with(writer, "{};2;{};{};{}", fg ? 38 : 48, color.r, color.g, color.b);
        else
            format_with(writer, "{}", (fg ? fg_table : bg_table)[(int)(char)color.color]);
    };

    if (m_active_face == face)
        return;

    writer.write("\033[");
    bool join = false;
    if (face.attributes != m_active_face.attributes)
    {
        for (int i = 0; i < std::size(attr_table); ++i)
        {
            if (face.attributes & (Attribute)(1 << i))
                format_with(writer, ";{}", attr_table[i]);
        }
        m_active_face.fg = m_active_face.bg = Color::Default;
        join = true;
    }
    if (m_active_face.fg != face.fg)
    {
        set_color(true, face.fg, join);
        join = true;
    }
    if (m_active_face.bg != face.bg)
    {
        set_color(false, face.bg, join);
        join = true;
    }
    if (m_active_face.underline != face.underline)
    {
        if (join)
            writer.write(";");
        if (face.underline != Color::Default)
        {
            if (face.underline.isRGB())
                format_with(writer, "58:2::{}:{}:{}", face.underline.r, face.underline.g, face.underline.b);
            else
                format_with(writer, "58:5:{}", ul_table[(int)(char)face.underline.color]);
        }
        else
            format_with(writer, "59");
    }
    writer.write("m");

    m_active_face = face;
}

void TerminalUI::Screen::output(bool force, bool synchronized, Writer& writer)
{
    if (not lines)
        return;

    if (force)
    {
        std::fill_n(hashes.get(), (size_t)size.line, 0);
        writer.write("\033[m");
        m_active_face = Face{};
    }

    auto hash_line = [](const Line& line) {
        return (hash_value(line.atoms) << 1) | 1; // ensure non-zero
    };

    auto output_line = [&](const Line& line) {
        ColumnCount pending_move = 0;
        for (auto& [text, skip, face] : line.atoms)
        {
            if (text.empty() and skip == 0)
                continue;

            if (pending_move != 0)
            {
                format_with(writer, "\033[{}C", (int)pending_move);
                pending_move = 0;
            }
            set_face(face, writer);
            writer.write(text);
            if (skip > 3 and face.attributes == Attribute{})
            {
                writer.write("\033[K");
                pending_move = skip;
            }
            else if (skip > 0)
                writer.write(String{' ', skip});
        }
    };

    if (synchronized)
    {
        writer.write("\033[?2026h"); // begin synchronized update

        struct Change { int keep; int add; int del; };
        Vector<Change> changes{Change{}};
        auto new_hashes = ArrayView{lines.get(), (size_t)size.line} | transform(hash_line);
        for_each_diff(hashes.get(), (int)size.line,
                      new_hashes.begin(), (int)size.line,
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
        std::copy(new_hashes.begin(), new_hashes.end(), hashes.get());

        int line = 0;
        for (auto& change : changes)
        {
            line += change.keep;
            if (int del = change.del - change.add; del > 0)
            {
                format_with(writer, "\033[{}H\033[{}M", line + 1, del);
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
                if (int add = change.add - change.del; i == 0 and add > 0)
                    format_with(writer, "\033[{}H\033[{}L", line + 1, add);
                else
                    format_with(writer, "\033[{}H", line + 1);

                output_line(lines[line++]);
            }
        }

        writer.write("\033[?2026l"); // end synchronized update
    }
    else
    {
        for (int line = 0; line < (int)size.line; ++line)
        {
            auto hash = hash_line(lines[line]); 
            if (hash == hashes[line])
                continue;
            hashes[line] = hash;

            format_with(writer, "\033[{}H", line + 1);
            output_line(lines[line]);
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
static sig_atomic_t stdin_closed = 0;

template<sig_atomic_t* signal_flag>
static void signal_handler(int)
{
    *signal_flag = 1;
    EventManager::instance().force_signal(0);
}

TerminalUI::TerminalUI()
    : m_cursor{CursorMode::Buffer, {}},
      m_stdin_watcher{STDIN_FILENO, FdEvents::Read, EventMode::Urgent,
                      [this](FDWatcher&, FdEvents, EventMode) {
        if (not m_on_key)
            return;

        while (auto key = get_next_key())
        {
            if (key == ctrl('z'))
                kill(0, SIGTSTP); // We suspend at this line
            else
                m_on_key(*key);
        }
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
    set_signal_handler(SIGHUP, &signal_handler<&stdin_closed>);
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
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_original_termios);
    restore_terminal();

    auto current = set_signal_handler(SIGTSTP, SIG_DFL);
    sigset_t unblock_sigtstp, old_mask;
    sigemptyset(&unblock_sigtstp);
    sigaddset(&unblock_sigtstp, SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &unblock_sigtstp, &old_mask);

    raise(SIGTSTP); // suspend here

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

    tcsetattr(STDIN_FILENO, TCSANOW, &attr);
}

void TerminalUI::redraw(bool force)
{
    m_window.blit(m_screen);

    if (m_menu.columns != 0 or m_menu.pos.column > m_status_len)
        m_menu.blit(m_screen);

    m_info.blit(m_screen);

    Writer writer{STDOUT_FILENO};
    m_screen.output(force, (bool)m_synchronized, writer);

    auto set_cursor_pos = [&](DisplayCoord c) {
        format_with(writer, "\033[{};{}H", (int)c.line + 1, (int)c.column + 1);
    };
    if (m_cursor.mode == CursorMode::Prompt)
        set_cursor_pos({m_status_on_top ? 0 : m_dimensions.line, m_cursor.coord.column});
    else
        set_cursor_pos(m_cursor.coord + content_line_offset());
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

    DisplayAtom padding{String{m_padding_char, m_padding_fill ? dim.column : 1}};

    while (line_index < dim.line + line_offset)
        m_window.draw(line_index++, padding, face);

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
        Writer writer{STDOUT_FILENO};
        constexpr char suffix[] = " - Kakoune\007";
        writer.write("\033]2;");
        // Fill title escape sequence buffer, removing non ascii characters
        for (auto& atom : mode_line)
        {
            const auto str = atom.content();
            for (auto it = str.begin(), end = str.end(); it != end; utf8::to_next(it, end))
                writer.write((*it >= 0x20 and *it <= 0x7e) ? *it : '?');
        }
        writer.write(suffix);
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

    DisplayCoord terminal_size{24_line, 80_col};
    if (winsize ws; ioctl(fd, TIOCGWINSZ, &ws) == 0 and ws.ws_row > 0 and ws.ws_col > 0)
        terminal_size = {ws.ws_row, ws.ws_col};

    const bool info = (bool)m_info;
    const bool menu = (bool)m_menu;
    if (m_window) m_window.destroy();
    if (info) m_info.destroy();
    if (menu) m_menu.destroy();

    m_window.create({0, 0}, terminal_size);
    m_screen.create({0, 0}, terminal_size);
    m_screen.hashes.reset(new size_t[(int)terminal_size.line]{});
    kak_assert(m_window);

    m_dimensions = terminal_size - 1_line;

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
    if (stdin_closed)
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
        if (not fd_readable(STDIN_FILENO))
            return {};

        if (unsigned char c = 0; read(STDIN_FILENO, &c, 1) == 1)
            return c;

        stdin_closed = 1;
        return {};
    };

    const auto c = get_char();
    if (not c)
        return {};

    static constexpr auto control = [](char c) { return c & 037; };

    auto convert = [this](Codepoint c) -> Codepoint {
        if (c == control('m') or c == control('j'))
            return Key::Return;
        if (c == control('i'))
            return Key::Tab;
        if (c == ' ')
            return Key::Space;
        if (c == m_original_termios.c_cc[VERASE])
            return Key::Backspace;
        if (c == 127) // when it's not backspace
            return Key::Delete;
        if (c == 27)
            return Key::Escape;
        return c;
    };
    auto parse_key = [&convert](unsigned char c) -> Key {
        if (Codepoint cp = convert(c); cp > 255)
            return Key{cp};
        // Special case: you can type NUL with Ctrl-2 or Ctrl-Shift-2 or
        // Ctrl-Backtick, but the most straightforward way is Ctrl-Space.
        if (c == 0)
            return ctrl(Key::Space);
        // Represent Ctrl-letter combinations in lower-case, to be clear
        // that Shift is not involved.
        if (c < 27)
            return ctrl(c - 1 + 'a');
        // Represent Ctrl-symbol combinations in "upper-case", as they are
        // traditionally-rendered.
        // Note that Escape is handled elsewhere.
        if (c < 32)
            return ctrl(c - 1 + 'A');

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

    auto parse_csi = [this, &convert, &parse_mask]() -> Optional<Key> {
        auto next_char = [] { return get_char().value_or((unsigned char)0xff); };
        int params[16][4] = {};
        auto c = next_char();
        char private_mode = 0;
        if (c == '?' or c == '<' or c == '=' or c == '>')
        {
            private_mode = c;
            c = next_char();
        }
        for (int count = 0, subcount = 0; count < 16 and c >= 0x30 && c <= 0x3f; c = next_char())
        {
            if (isdigit(c))
                params[count][subcount] = params[count][subcount] * 10 + c - '0';
            else if (c == ':' && subcount < 3)
                ++subcount;
            else if (c == ';')
            {
                ++count;
                subcount = 0;
            }
            else
                return {};
        }
        if (c != '$' and (c < 0x40 or c > 0x7e))
            return {};

        auto mouse_button = [this](Key::Modifiers mod, Key::MouseButton button, Codepoint coord, bool release) {
            auto mask = 1 << (int)button;
            if (not release)
            {
                mod |= (m_mouse_state & mask) ? Key::Modifiers::MousePos : Key::Modifiers::MousePress;
                m_mouse_state |= mask;
            }
            else
            {
                mod |= Key::Modifiers::MouseRelease;
                m_mouse_state &= ~mask;
            }
            return Key{mod | Key::to_modifier(button), coord};
        };

        auto mouse_scroll = [this](Key::Modifiers mod, bool down) -> Key {
            return {mod | Key::Modifiers::Scroll,
                    (Codepoint)((down ? 1 : -1) * m_wheel_scroll_amount)};
        };

        auto masked_key = [&](Codepoint key, Codepoint shifted_key = 0) {
            int mask = std::max(params[1][0] - 1, 0);
            Key::Modifiers modifiers = parse_mask(mask);
            if (shifted_key != 0 and (modifiers & Key::Modifiers::Shift))
            {
                modifiers &= ~Key::Modifiers::Shift;
                key = shifted_key;
            }
            return Key{modifiers, key};
        };

        switch (c)
        {
        case '$':
            if (private_mode == '?' and next_char() == 'y') // DECRPM
            {
                if (params[0][0] == 2026)
                    m_synchronized.supported = (params[1][0] == 1 or params[1][0] == 2);
                return {Key::Invalid};
            }
            switch (params[0][0])
            {
            case 23: case 24:
                return Key{Key::Modifiers::Shift, Key::F11 + params[0][0] - 23}; // rxvt style
            }
            return {};
        case 'A': return masked_key(Key::Up);
        case 'B': return masked_key(Key::Down);
        case 'C': return masked_key(Key::Right);
        case 'D': return masked_key(Key::Left);
        case 'E': return masked_key('5');        // Numeric keypad 5
        case 'F': return masked_key(Key::End);   // PC/xterm style
        case 'H': return masked_key(Key::Home);  // PC/xterm style
        case 'P': return masked_key(Key::F1);
        case 'Q': return masked_key(Key::F2);
        case 'R': return masked_key(Key::F3);
        case 'S': return masked_key(Key::F4);
        case '~':
            switch (params[0][0])
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
                return masked_key(Key::F1 + params[0][0] - 11);
            case 17: case 18: case 19: case 20: case 21:
                return masked_key(Key::F6 + params[0][0] - 17);
            case 23: case 24:
                return masked_key(Key::F11 + params[0][0] - 23);
            case 25: case 26:
                return Key{Key::Modifiers::Shift, Key::F3 + params[0][0] - 25}; // rxvt style
            case 28: case 29:
                return Key{Key::Modifiers::Shift, Key::F5 + params[0][0] - 28}; // rxvt style
            case 31: case 32:
                return Key{Key::Modifiers::Shift, Key::F7 + params[0][0] - 31}; // rxvt style
            case 33: case 34:
                return Key{Key::Modifiers::Shift, Key::F9 + params[0][0] - 33}; // rxvt style
            }
            return {};
        case 'u':
            return masked_key(convert(static_cast<Codepoint>(params[0][0])),
                              convert(static_cast<Codepoint>(params[0][1])));
        case 'Z': return shift(Key::Tab);
        case 'I': return {Key::FocusIn};
        case 'O': return {Key::FocusOut};
        case 'M': case 'm':
            const bool sgr = private_mode == '<';
            if (not sgr and c != 'M')
                return {};

            const Codepoint b = sgr ? params[0][0] : next_char() - 32;
            const int x = (sgr ? params[1][0] : next_char() - 32) - 1;
            const int y = (sgr ? params[2][0] : next_char() - 32) - 1;
            auto coord = encode_coord({y - content_line_offset(), x});
            Key::Modifiers mod = parse_mask((b >> 2) & 0x7);
            switch (const int code = b & 0x43; code)
            {
            case 0: case 1: case 2:
                return mouse_button(mod, Key::MouseButton{code}, coord, c == 'm');
            case 3:
                if (sgr)
                    return {};
                else if (int guess = ffs(m_mouse_state) - 1; 0 <= guess and guess < 3)
                    return mouse_button(mod, Key::MouseButton{guess}, coord, true);
                break;
            case 64: return mouse_scroll(mod, false);
            case 65: return mouse_scroll(mod, true);
            }
            return Key{Key::Modifiers::MousePos, coord};
        }
        return {};
    };

    auto parse_ss3 = [&parse_mask]() -> Optional<Key> {
        int raw_mask = 0;
        char code = '0';
        do {
            raw_mask = raw_mask * 10 + (code - '0');
            code = get_char().value_or((unsigned char)0xff);
        } while (code >= '0' and code <= '9');
        Key::Modifiers mod = parse_mask(std::max(raw_mask - 1, 0));

        switch (code)
        {
        case ' ': return Key{mod, Key::Space};
        case 'A': return Key{mod, Key::Up};
        case 'B': return Key{mod, Key::Down};
        case 'C': return Key{mod, Key::Right};
        case 'D': return Key{mod, Key::Left};
        case 'F': return Key{mod, Key::End};
        case 'H': return Key{mod, Key::Home};
        case 'I': return Key{mod, Key::Tab};
        case 'M': return Key{mod, Key::Return};
        case 'P': return Key{mod, Key::F1};
        case 'Q': return Key{mod, Key::F2};
        case 'R': return Key{mod, Key::F3};
        case 'S': return Key{mod, Key::F4};
        case 'X': return Key{mod, '='};
        case 'j': return Key{mod, '*'};
        case 'k': return Key{mod, '+'};
        case 'l': return Key{mod, ','};
        case 'm': return Key{mod, '-'};
        case 'n': return Key{mod, '.'};
        case 'o': return Key{mod, '/'};
        case 'p': return Key{mod, '0'};
        case 'q': return Key{mod, '1'};
        case 'r': return Key{mod, '2'};
        case 's': return Key{mod, '3'};
        case 't': return Key{mod, '4'};
        case 'u': return Key{mod, '5'};
        case 'v': return Key{mod, '6'};
        case 'w': return Key{mod, '7'};
        case 'x': return Key{mod, '8'};
        case 'y': return Key{mod, '9'};
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

    if (height == 0)
        return;

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
    else if (line + height > m_dimensions.line and anchor.line >= height)
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
    write(STDOUT_FILENO,
        "\033[?1049h" // enable alternative screen buffer
        "\033[?1004h" // enable focus notify
        "\033[>4;1m"  // request CSI u style key reporting
        "\033[>5u"    // kitty progressive enhancement - report shifted key codes
        "\033[22t"    // save the current window title
        "\033[?25l"   // hide cursor
        "\033="       // set application keypad mode, so the keypad keys send unique codes
        "\033[?2026$p" // query support for synchronize output
    );
}

void TerminalUI::restore_terminal()
{
    write(STDOUT_FILENO,
        "\033>"
        "\033[?25h"
        "\033[23t"
        "\033[<u"
        "\033[>4;0m"
        "\033[?1004l"
        "\033[?1049l"
        "\033[m" // set the terminal output back to default colours and style
    );
}

void TerminalUI::enable_mouse(bool enabled)
{
    if (enabled == m_mouse_enabled)
        return;

    m_mouse_enabled = enabled;
    if (enabled)
    {
        write(STDOUT_FILENO,
            "\033[?1006h" // force SGR mode
            "\033[?1000h" // enable mouse
            "\033[?1002h" // force enable report mouse position
        );
    }
    else
    {
        write(STDOUT_FILENO,
            "\033[?1002l"
            "\033[?1000l"
            "\033[?1006l"
        );
    }
}

void TerminalUI::set_ui_options(const Options& options)
{
    auto find = [&](StringView name) -> Optional<StringView> {
        if (auto it = options.find(name); it != options.end())
            return StringView{it->value};
        return {};
    };

    auto assistant = find("terminal_assistant").value_or("clippy");
    if (assistant == "clippy")
        m_assistant = assistant_clippy;
    else if (assistant == "cat")
        m_assistant = assistant_cat;
    else if (assistant == "dilbert")
        m_assistant = assistant_dilbert;
    else if (assistant == "none" or assistant == "off")
        m_assistant = ConstArrayView<StringView>{};

    auto to_bool = [](StringView s) { return s == "yes" or s == "true"; };

    m_status_on_top = find("terminal_status_on_top").map(to_bool).value_or(false);
    m_set_title = find("terminal_set_title").map(to_bool).value_or(true);

    auto synchronized = find("terminal_synchronized").map(to_bool);
    m_synchronized.set = (bool)synchronized;
    m_synchronized.requested = synchronized.value_or(false);

    m_shift_function_key = find("terminal_shift_function_key").map(str_to_int_ifp).value_or(default_shift_function_key);

    enable_mouse(find("terminal_enable_mouse").map(to_bool).value_or(true));
    m_wheel_scroll_amount = find("terminal_wheel_scroll_amount").map(str_to_int_ifp).value_or(3);

    m_padding_char = find("terminal_padding_char").map([](StringView s) { return s.column_length() < 1 ? ' ' : s[0_char]; }).value_or(Codepoint{'~'});
    m_padding_fill = find("terminal_padding_fill").map(to_bool).value_or(false);
}

}
