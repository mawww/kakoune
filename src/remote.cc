#include "remote.hh"

#include "buffer_manager.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "debug.hh"
#include "display_buffer.hh"
#include "event_manager.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>


namespace Kakoune
{

enum class RemoteUIMsg
{
    PrintStatus,
    MenuShow,
    MenuSelect,
    MenuHide,
    InfoShow,
    InfoHide,
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

    template<typename T>
    void write(const T& val)
    {
        write((const char*)&val, sizeof(val));
    }

    void write(const String& str)
    {
        write(str.length());
        write(str.c_str(), (int)str.length());
    };

    template<typename T>
    void write(const memoryview<T>& view)
    {
        write<uint32_t>(view.size());
        for (auto& val : view)
            write(val);
    }

    template<typename T>
    void write(const std::vector<T>& vec)
    {
        write(memoryview<T>(vec));
    }

    void write(const Color& color)
    {
        write(color.color);
        if (color.color == Colors::RGB)
        {
            write(color.r);
            write(color.g);
            write(color.b);
        }
    }

    void write(const ColorPair& colors)
    {
        write(colors.first);
        write(colors.second);
    }

    void write(const DisplayAtom& atom)
    {
        write(atom.content.content());
        write(atom.colors);
        write(atom.attribute);
    }

    void write(const DisplayLine& line)
    {
        write(line.atoms());
    }

    void write(const DisplayBuffer& display_buffer)
    {
        write(display_buffer.lines());
    }

private:
    std::vector<char> m_stream;
    int m_socket;
};

void read(int socket, char* buffer, size_t size)
{
    while (size)
    {
        int res = ::read(socket, buffer, size);
        if (res == 0)
            throw peer_disconnected{};
        if (res < 0)
            throw socket_error{};

        buffer += res;
        size   -= res;
    }
}

template<typename T>
T read(int socket)
{
    char value[sizeof(T)];
    read(socket, value, sizeof(T));
    return *(T*)(value);
}

template<>
String read<String>(int socket)
{
    ByteCount length = read<ByteCount>(socket);
    if (length == 0)
        return String{};
    char buffer[2048];
    kak_assert(length < 2048);
    read(socket, buffer, (int)length);
    return String(buffer, buffer+(int)length);
}

template<typename T>
std::vector<T> read_vector(int socket)
{
    uint32_t size = read<uint32_t>(socket);
    std::vector<T> res;
    res.reserve(size);
    while (size--)
        res.push_back(read<T>(socket));
    return res;
}

template<>
Color read<Color>(int socket)
{
    Color res;
    res.color = read<Colors>(socket);
    if (res.color == Colors::RGB)
    {
        res.r = read<unsigned char>(socket);
        res.g = read<unsigned char>(socket);
        res.b = read<unsigned char>(socket);
    }
    return res;
}

template<>
ColorPair read<ColorPair>(int socket)
{
    ColorPair res;
    res.first = read<Color>(socket);
    res.second = read<Color>(socket);
    return res;
}

template<>
DisplayAtom read<DisplayAtom>(int socket)
{
    DisplayAtom atom(AtomContent(read<String>(socket)));
    atom.colors = read<ColorPair>(socket);
    atom.attribute = read<Attribute>(socket);
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

class RemoteUI : public UserInterface
{
public:
    RemoteUI(int socket);
    ~RemoteUI();

    void print_status(const DisplayLine& status) override;

    void menu_show(const memoryview<String>& choices,
                   DisplayCoord anchor, ColorPair fg, ColorPair bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const String& content, DisplayCoord anchor,
                   ColorPair colors, MenuStyle style) override;
    void info_hide() override;

    void draw(const DisplayBuffer& display_buffer,
              const DisplayLine& mode_line) override;

    bool is_key_available() override;
    Key  get_key() override;
    DisplayCoord dimensions() override;

    void set_input_callback(InputCallback callback) override;

private:
    FDWatcher    m_socket_watcher;
    DisplayCoord m_dimensions;
    InputCallback m_input_callback;
};


RemoteUI::RemoteUI(int socket)
    : m_socket_watcher(socket, [this](FDWatcher&) { if (m_input_callback) m_input_callback(); })
{
    write_debug("remote client connected: " + int_to_str(m_socket_watcher.fd()));
}

RemoteUI::~RemoteUI()
{
    write_debug("remote client disconnected: " + int_to_str(m_socket_watcher.fd()));
    close(m_socket_watcher.fd());
}

void RemoteUI::print_status(const DisplayLine& status)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::PrintStatus);
    msg.write(status);
}

void RemoteUI::menu_show(const memoryview<String>& choices,
                         DisplayCoord anchor, ColorPair fg, ColorPair bg,
                         MenuStyle style)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::MenuShow);
    msg.write(choices);
    msg.write(anchor);
    msg.write(fg);
    msg.write(bg);
    msg.write(style);
}

void RemoteUI::menu_select(int selected)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::MenuSelect);
    msg.write(selected);
}

void RemoteUI::menu_hide()
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::MenuHide);
}

void RemoteUI::info_show(const String& content, DisplayCoord anchor,
                         ColorPair colors, MenuStyle style)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::InfoShow);
    msg.write(content);
    msg.write(anchor);
    msg.write(colors);
    msg.write(style);
}

void RemoteUI::info_hide()
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::InfoHide);
}

void RemoteUI::draw(const DisplayBuffer& display_buffer,
                    const DisplayLine& mode_line)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::Draw);
    msg.write(display_buffer);
    msg.write(mode_line);
}

static const Key::Modifiers resize_modifier = (Key::Modifiers)0x80;

bool RemoteUI::is_key_available()
{
    timeval tv;
    fd_set  rfds;

    int sock = m_socket_watcher.fd();
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int res = select(sock+1, &rfds, NULL, NULL, &tv);
    return res == 1;
}

Key RemoteUI::get_key()
{
    Key key = read<Key>(m_socket_watcher.fd());
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

void RemoteUI::set_input_callback(InputCallback callback)
{
    m_input_callback = std::move(callback);
}

RemoteClient::RemoteClient(int socket, std::unique_ptr<UserInterface>&& ui,
                           const String& init_command)
    : m_ui(std::move(ui)), m_dimensions(m_ui->dimensions()),
      m_socket_watcher{socket, [this](FDWatcher&){ process_next_message(); }}
{
    Message msg(socket);
    msg.write(init_command.c_str(), (int)init_command.length()+1);
    Key key{ resize_modifier, Codepoint(((int)m_dimensions.line << 16) | (int)m_dimensions.column) };
    msg.write(key);

    m_ui->set_input_callback([this]{ write_next_key(); });
}

void RemoteClient::process_next_message()
{
    int socket = m_socket_watcher.fd();
    RemoteUIMsg msg = read<RemoteUIMsg>(socket);
    switch (msg)
    {
    case RemoteUIMsg::PrintStatus:
    {
        auto status = read<DisplayLine>(socket);
        m_ui->print_status(status);
        break;
    }
    case RemoteUIMsg::MenuShow:
    {
        auto choices = read_vector<String>(socket);
        auto anchor = read<DisplayCoord>(socket);
        auto fg = read<ColorPair>(socket);
        auto bg = read<ColorPair>(socket);
        auto style = read<MenuStyle>(socket);
        m_ui->menu_show(choices, anchor, fg, bg, style);
        break;
    }
    case RemoteUIMsg::MenuSelect:
        m_ui->menu_select(read<int>(socket));
        break;
    case RemoteUIMsg::MenuHide:
        m_ui->menu_hide();
        break;
    case RemoteUIMsg::InfoShow:
    {
        auto choices = read<String>(socket);
        auto anchor = read<DisplayCoord>(socket);
        auto colors = read<ColorPair>(socket);
        auto style = read<MenuStyle>(socket);
        m_ui->info_show(choices, anchor, colors, style);
        break;
    }
    case RemoteUIMsg::InfoHide:
        m_ui->info_hide();
        break;
    case RemoteUIMsg::Draw:
    {
        auto display_buffer = read<DisplayBuffer>(socket);
        auto mode_line = read<DisplayLine>(socket);
        m_ui->draw(display_buffer, mode_line);
        break;
    }
    }
}

void RemoteClient::write_next_key()
{
    Message msg(m_socket_watcher.fd());
    // do that before checking dimensions as get_key may
    // handle a resize event.
    msg.write(m_ui->get_key());

    DisplayCoord dimensions = m_ui->dimensions();
    if (dimensions != m_dimensions)
    {
        m_dimensions = dimensions;
        Key key{ resize_modifier, Codepoint(((int)dimensions.line << 16) | (int)dimensions.column) };
        msg.write(key);
    }
}

std::unique_ptr<RemoteClient> connect_to(const String& pid, std::unique_ptr<UserInterface>&& ui,
                                         const String& init_command)
{
    auto filename = "/tmp/kak-" + pid;

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, filename.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) == -1)
        throw runtime_error("connect to " + filename + " failed");

    return std::unique_ptr<RemoteClient>{new RemoteClient{sock, std::move(ui), init_command}};
}


// A client accepter handle a connection until it closes or a nul byte is
// recieved. Everything recieved before is considered to be a command.
//
// * When a nul byte is recieved, the socket is handed to a new Client along
//   with the command.
// * When the connection is closed, the command is run in an empty context.
class ClientAccepter
{
public:
    ClientAccepter(int socket)
        : m_socket_watcher(socket, [this](FDWatcher&) { handle_available_input(); }) {}

private:
    void handle_available_input()
    {
        int socket = m_socket_watcher.fd();
        timeval tv{ 0, 0 };
        fd_set  rfds;
        do
        {
            char c;
            int res = ::read(socket, &c, 1);
            if (res <= 0)
            {
                if (not m_buffer.empty()) try
                {
                    Context context{};
                    CommandManager::instance().execute(m_buffer, context);
                }
                catch (runtime_error& e)
                {
                    write_debug("error running command '" + m_buffer + "' : " + e.what());
                }
                catch (client_removed&) {}
                ClientManager::instance().redraw_clients();
                close(socket);
                delete this;
                return;
            }
            if (c == 0) // end of initial command stream, go to interactive ui mode
            {
                ClientManager::instance().create_client(
                    std::unique_ptr<UserInterface>{new RemoteUI{socket}}, m_buffer);
                delete this;
                return;
            }
            else
                m_buffer += c;

            FD_ZERO(&rfds);
            FD_SET(socket, &rfds);
        }
        while (select(socket+1, &rfds, NULL, NULL, &tv) == 1);
    }

    String    m_buffer;
    FDWatcher m_socket_watcher;
};

Server::Server()
    : m_filename{"/tmp/kak-" + int_to_str(getpid())}
{
    int listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, m_filename.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(listen_sock, (sockaddr*) &addr, sizeof(sockaddr_un)) == -1)
       throw runtime_error("unable to bind listen socket " + m_filename);

    if (listen(listen_sock, 4) == -1)
       throw runtime_error("unable to listen on socket " + m_filename);

    auto accepter = [](FDWatcher& watcher) {
        sockaddr_un client_addr;
        socklen_t   client_addr_len = sizeof(sockaddr_un);
        int sock = accept(watcher.fd(), (sockaddr*) &client_addr, &client_addr_len);
        if (sock == -1)
            throw runtime_error("accept failed");
        fcntl(sock, F_SETFD, FD_CLOEXEC);

        new ClientAccepter{sock};
    };
    m_listener.reset(new FDWatcher{listen_sock, accepter});
}

Server::~Server()
{
    unlink(m_filename.c_str());
    close(m_listener->fd());
}

}
