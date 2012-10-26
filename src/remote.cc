#include "remote.hh"

#include "display_buffer.hh"

#include <sys/types.h>
#include <sys/socket.h>

namespace Kakoune
{

enum class RemoteUIMsg
{
    PrintStatus,
    MenuShow,
    MenuSelect,
    MenuHide,
    Draw
};

struct socket_error{};

class Message
{
public:
    Message(int sock) : m_socket(sock) {}
    ~Message()
    {
        if (m_stream.size() == 0)
            return;
        int res = ::write(m_socket, m_stream.data(), m_stream.size());
        if (res == 0)
            throw peer_disconnected{};
    }

    void write(const char* val, size_t size)
    {
        m_stream.insert(m_stream.end(), val, val + size);
    }

private:
    std::vector<char> m_stream;
    int m_socket;
};

template<typename T>
void write(Message& msg, const T& val)
{
    msg.write((const char*)&val, sizeof(val));
};

void write(Message& msg, const String& str)
{
    write(msg, str.length());
    msg.write(str.c_str(), (int)str.length());
};

template<typename T>
void write(Message& msg, const memoryview<T>& view)
{
    write<uint32_t>(msg, view.size());
    for (auto& val : view)
        write(msg, val);
};

template<typename T>
void write(Message& msg, const std::vector<T>& vec)
{
    write(msg, memoryview<T>(vec));
}

void write(Message& msg, const DisplayAtom& atom)
{
    write(msg, atom.fg_color);
    write(msg, atom.bg_color);
    write(msg, atom.attribute);
    write(msg, atom.content.content());
}

void write(Message& msg, const DisplayLine& line)
{
    write(msg, line.atoms());
}

void write(Message& msg, const DisplayBuffer& display_buffer)
{
    write(msg, display_buffer.lines());
}

void read(int socket, char* buffer, size_t size)
{
    int res = ::read(socket, buffer, size);
    if (res == 0)
        throw peer_disconnected{};
    if (res == -1)
        throw socket_error{};
    assert(res == size);
}

template<typename T>
T read(int socket)
{
    char value[sizeof(T)];
    read(socket, value, sizeof(T));
    return *(T*)(value);
};

template<>
String read<String>(int socket)
{
    ByteCount length = read<ByteCount>(socket);
    char buffer[2048];
    assert(length < 2048);
    read(socket, buffer, (int)length);
    return String(buffer, buffer+(int)length);
};

template<typename T>
std::vector<T> read_vector(int socket)
{
    uint32_t size = read<uint32_t>(socket);
    std::vector<T> res;
    res.reserve(size);
    while (size--)
        res.push_back(read<T>(socket));
    return res;
};

template<>
DisplayAtom read<DisplayAtom>(int socket)
{
    Color fg_color = read<Color>(socket);
    Color bg_color = read<Color>(socket);
    Attribute attribute = read<Attribute>(socket);
    DisplayAtom atom(AtomContent(read<String>(socket)));
    atom.fg_color = fg_color;
    atom.bg_color = bg_color;
    atom.attribute = attribute;
    return atom;
}
template<>
DisplayLine read<DisplayLine>(int socket)
{
    return DisplayLine(0, read_vector<DisplayAtom>(socket));
}

template<>
DisplayBuffer read<DisplayBuffer>(int socket)
{
    DisplayBuffer db;
    db.lines() = read_vector<DisplayLine>(socket);
    return db;
}

void RemoteUI::print_status(const String& status, CharCount cursor_pos)
{
    Message msg(m_socket);
    write(msg, RemoteUIMsg::PrintStatus);
    write(msg, status);
    write(msg, cursor_pos);
}

void RemoteUI::menu_show(const memoryview<String>& choices,
                         const DisplayCoord& anchor, MenuStyle style)
{
    Message msg(m_socket);
    write(msg, RemoteUIMsg::MenuShow);
    write(msg, choices);
    write(msg, anchor);
    write(msg, style);
}

void RemoteUI::menu_select(int selected)
{
    Message msg(m_socket);
    write(msg, RemoteUIMsg::MenuSelect);
    write(msg, selected);
}

void RemoteUI::menu_hide()
{
    Message msg(m_socket);
    write(msg, RemoteUIMsg::MenuHide);
}

void RemoteUI::draw(const DisplayBuffer& display_buffer,
                    const String& status_line)
{
    Message msg(m_socket);
    write(msg, RemoteUIMsg::Draw);
    write(msg, display_buffer);
    write(msg, status_line);
}

static const Key::Modifiers resize_modifier = (Key::Modifiers)0x80;

Key RemoteUI::get_key()
{
    Key key = read<Key>(m_socket);
    if (key.modifiers == resize_modifier)
    {
        m_dimensions = { (int)(key.key >> 16), (int)(key.key & 0xFFFF) };
        return Key::Invalid;
    }
    return key;
}

DisplayCoord RemoteUI::dimensions()
{
    return m_dimensions;
}

RemoteClient::RemoteClient(int socket, UserInterface* ui)
    : m_socket(socket), m_ui(ui), m_dimensions(ui->dimensions())
{
     Key key{ resize_modifier, Codepoint(((int)m_dimensions.line << 16) | (int)m_dimensions.column) };
     Message msg(socket);
     write(msg, key);
}

void RemoteClient::process_next_message()
{
    RemoteUIMsg msg = read<RemoteUIMsg>(m_socket);
    switch (msg)
    {
    case RemoteUIMsg::PrintStatus:
    {
         auto status = read<String>(m_socket);
         auto cursor_pos = read<CharCount>(m_socket);
         m_ui->print_status(status, cursor_pos);
         break;
    }
    case RemoteUIMsg::MenuShow:
    {
         auto choices = read_vector<String>(m_socket);
         auto anchor = read<DisplayCoord>(m_socket);
         auto style = read<MenuStyle>(m_socket);
         m_ui->menu_show(choices, anchor, style);
         break;
    }
    case RemoteUIMsg::MenuSelect:
         m_ui->menu_select(read<int>(m_socket));
         break;
    case RemoteUIMsg::MenuHide:
         m_ui->menu_hide();
         break;
    case RemoteUIMsg::Draw:
    {
         DisplayBuffer display_buffer = read<DisplayBuffer>(m_socket);
         String status_line = read<String>(m_socket);
         m_ui->draw(display_buffer, status_line);
         break;
    }
    }
}

void RemoteClient::write_next_key()
{
    DisplayCoord dimensions = m_ui->dimensions();
    Message msg(m_socket);
    if (dimensions != m_dimensions)
    {
        m_dimensions = dimensions;
        Key key{ resize_modifier, Codepoint(((int)dimensions.line << 16) | (int)dimensions.column) };
        write(msg, m_dimensions);
    }
    write(msg, m_ui->get_key());
}

}
