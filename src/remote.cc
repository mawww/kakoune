#include "remote.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "file.hh"

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
    MenuShow,
    MenuSelect,
    MenuHide,
    InfoShow,
    InfoHide,
    Draw,
    DrawStatus,
    Refresh,
    SetOptions
};

struct socket_error{};

class Message
{
public:
    Message(int sock) : m_socket(sock) {}
    ~Message() noexcept(false)
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

    void write(StringView str)
    {
        write(str.length());
        write(str.data(), (int)str.length());
    };

    void write(const String& str)
    {
        write(StringView{str});
    }

    template<typename T>
    void write(ConstArrayView<T> view)
    {
        write<uint32_t>(view.size());
        for (auto& val : view)
            write(val);
    }

    template<typename T, MemoryDomain domain>
    void write(const Vector<T, domain>& vec)
    {
        write(ConstArrayView<T>(vec));
    }

    template<typename Key, typename Val, MemoryDomain domain>
    void write(const UnorderedMap<Key, Val, domain>& map)
    {
        write<uint32_t>(map.size());
        for (auto& val : map)
        {
            write(val.first);
            write(val.second);
        }
    }

    void write(Color color)
    {
        write(color.color);
        if (color.color == Color::RGB)
        {
            write(color.r);
            write(color.g);
            write(color.b);
        }
    }

    void write(Face face)
    {
        write(face.fg);
        write(face.bg);
        write(face.attributes);
    }

    void write(const DisplayAtom& atom)
    {
        write(atom.content());
        write(atom.face);
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
    Vector<char> m_stream;
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
    union U
    {
        T object;
        alignas(T) char data[sizeof(T)];
        U() {}
        ~U() { object.~T(); }
    } u;
    read(socket, u.data, sizeof(T));
    return u.object;
}

template<>
String read<String>(int socket)
{
    ByteCount length = read<ByteCount>(socket);
    String res;
    if (length > 0)
    {
        res.resize((int)length);
        read(socket, &res[0_byte], (int)length);
    }
    return res;
}

template<typename T>
Vector<T> read_vector(int socket)
{
    uint32_t size = read<uint32_t>(socket);
    Vector<T> res;
    res.reserve(size);
    while (size--)
        res.push_back(read<T>(socket));
    return res;
}

template<>
Color read<Color>(int socket)
{
    Color res;
    res.color = read<Color::NamedColor>(socket);
    if (res.color == Color::RGB)
    {
        res.r = read<unsigned char>(socket);
        res.g = read<unsigned char>(socket);
        res.b = read<unsigned char>(socket);
    }
    return res;
}

template<>
Face read<Face>(int socket)
{
    Face res;
    res.fg = read<Color>(socket);
    res.bg = read<Color>(socket);
    res.attributes = read<Attribute>(socket);
    return res;
}

template<>
DisplayAtom read<DisplayAtom>(int socket)
{
    DisplayAtom atom(read<String>(socket));
    atom.face = read<Face>(socket);
    return atom;
}
template<>
DisplayLine read<DisplayLine>(int socket)
{
    return DisplayLine(read_vector<DisplayAtom>(socket));
}

template<>
DisplayBuffer read<DisplayBuffer>(int socket)
{
    DisplayBuffer db;
    db.lines() = read_vector<DisplayLine>(socket);
    return db;
}

template<typename Key, typename Val, MemoryDomain domain>
UnorderedMap<Key, Val, domain> read_map(int socket)
{
    uint32_t size = read<uint32_t>(socket);
    UnorderedMap<Key, Val, domain> res;
    while (size--)
    {
        auto key = read<Key>(socket);
        auto val = read<Val>(socket);
        res.insert({std::move(key), std::move(val)});
    }
    return res;
}

class RemoteUI : public UserInterface
{
public:
    RemoteUI(int socket);
    ~RemoteUI();

    void menu_show(ConstArrayView<String> choices,
                   CharCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(StringView title, StringView content,
                   CharCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    void refresh() override;

    bool is_key_available() override;
    Key  get_key() override;
    CharCoord dimensions() override;

    void set_input_callback(InputCallback callback) override;

    void set_ui_options(const Options& options) override;

private:
    FDWatcher    m_socket_watcher;
    CharCoord m_dimensions;
    InputCallback m_input_callback;
};


RemoteUI::RemoteUI(int socket)
    : m_socket_watcher(socket, [this](FDWatcher&, EventMode mode) {
                           if (m_input_callback)
                               m_input_callback(mode);
                       })
{
    write_to_debug_buffer(format("remote client connected: {}", m_socket_watcher.fd()));
}

RemoteUI::~RemoteUI()
{
    write_to_debug_buffer(format("remote client disconnected: {}", m_socket_watcher.fd()));
    m_socket_watcher.close_fd();
}

void RemoteUI::menu_show(ConstArrayView<String> choices,
                         CharCoord anchor, Face fg, Face bg,
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

void RemoteUI::info_show(StringView title, StringView content,
                         CharCoord anchor, Face face,
                         InfoStyle style)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::InfoShow);
    msg.write(title);
    msg.write(content);
    msg.write(anchor);
    msg.write(face);
    msg.write(style);
}

void RemoteUI::info_hide()
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::InfoHide);
}

void RemoteUI::draw(const DisplayBuffer& display_buffer,
                    const Face& default_face)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::Draw);
    msg.write(display_buffer);
    msg.write(default_face);
}

void RemoteUI::draw_status(const DisplayLine& status_line,
                           const DisplayLine& mode_line,
                           const Face& default_face)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::DrawStatus);
    msg.write(status_line);
    msg.write(mode_line);
    msg.write(default_face);
}

void RemoteUI::refresh()
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::Refresh);
}

void RemoteUI::set_ui_options(const Options& options)
{
    Message msg(m_socket_watcher.fd());
    msg.write(RemoteUIMsg::SetOptions);
    msg.write(options);
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
    int res = select(sock+1, &rfds, nullptr, nullptr, &tv);
    return res == 1;
}

Key RemoteUI::get_key()
{
    try
    {
        Key key = read<Key>(m_socket_watcher.fd());
        if (key.modifiers == Key::Modifiers::Resize)
            m_dimensions = key.coord();
        return key;
    }
    catch (peer_disconnected&)
    {
        throw client_removed{};
    }
    catch (socket_error&)
    {
        write_to_debug_buffer("ungraceful deconnection detected");
        throw client_removed{};
    }
}

CharCoord RemoteUI::dimensions()
{
    return m_dimensions;
}

void RemoteUI::set_input_callback(InputCallback callback)
{
    m_input_callback = std::move(callback);
}

static sockaddr_un session_addr(StringView session)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    format_to(addr.sun_path, "/tmp/kakoune/{}/{}", getlogin(), session);
    return addr;
}

static int connect_to(StringView session)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr = session_addr(session);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) == -1)
        throw connection_failed(addr.sun_path);
    return sock;
}

RemoteClient::RemoteClient(StringView session, std::unique_ptr<UserInterface>&& ui,
                           const EnvVarMap& env_vars, StringView init_command)
    : m_ui(std::move(ui))
{
    int sock = connect_to(session);

    {
        Message msg(sock);
        msg.write(init_command.data(), (int)init_command.length());
        msg.write((char)0);
        msg.write(env_vars);
    }

    m_ui->set_input_callback([this](EventMode){ write_next_key(); });

    m_socket_watcher.reset(new FDWatcher{sock, [this](FDWatcher&, EventMode){ process_available_messages(); }});
}

void RemoteClient::process_available_messages()
{
    int socket = m_socket_watcher->fd();
    timeval tv{ 0, 0 };
    fd_set  rfds;

    do {
        process_next_message();

        FD_ZERO(&rfds);
        FD_SET(socket, &rfds);
    } while (select(socket+1, &rfds, nullptr, nullptr, &tv) == 1);
}

void RemoteClient::process_next_message()
{
    int socket = m_socket_watcher->fd();
    RemoteUIMsg msg = read<RemoteUIMsg>(socket);
    switch (msg)
    {
    case RemoteUIMsg::MenuShow:
    {
        auto choices = read_vector<String>(socket);
        auto anchor = read<CharCoord>(socket);
        auto fg = read<Face>(socket);
        auto bg = read<Face>(socket);
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
        auto title = read<String>(socket);
        auto content = read<String>(socket);
        auto anchor = read<CharCoord>(socket);
        auto face = read<Face>(socket);
        auto style = read<InfoStyle>(socket);
        m_ui->info_show(title, content, anchor, face, style);
        break;
    }
    case RemoteUIMsg::InfoHide:
        m_ui->info_hide();
        break;
    case RemoteUIMsg::Draw:
    {
        auto display_buffer = read<DisplayBuffer>(socket);
        auto default_face = read<Face>(socket);
        m_ui->draw(display_buffer, default_face);
        break;
    }
    case RemoteUIMsg::DrawStatus:
    {
        auto status_line = read<DisplayLine>(socket);
        auto mode_line = read<DisplayLine>(socket);
        auto default_face = read<Face>(socket);
        m_ui->draw_status(status_line, mode_line, default_face);
        break;
    }
    case RemoteUIMsg::Refresh:
        m_ui->refresh();
        break;
    case RemoteUIMsg::SetOptions:
        m_ui->set_ui_options(read_map<String, String, MemoryDomain::Options>(socket));
        break;
    }
}

void RemoteClient::write_next_key()
{
    Message msg(m_socket_watcher->fd());
    // do that before checking dimensions as get_key may
    // handle a resize event.
    msg.write(m_ui->get_key());
}

void send_command(StringView session, StringView command)
{
    int sock = connect_to(session);
    ::write(sock, command.data(), (int)command.length());
    close(sock);
}


// A client accepter handle a connection until it closes or a nul byte is
// recieved. Everything recieved before is considered to be a command.
//
// * When a nul byte is recieved, the socket is handed to a new Client along
//   with the command.
// * When the connection is closed, the command is run in an empty context.
class Server::Accepter
{
public:
    Accepter(int socket)
        : m_socket_watcher(socket,
                           [this](FDWatcher&, EventMode mode) {
                               if (mode == EventMode::Normal)
                                   handle_available_input();
                           })
    {}

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
                    Context context{Context::EmptyContextFlag{}};
                    CommandManager::instance().execute(m_buffer, context);
                }
                catch (runtime_error& e)
                {
                    write_to_debug_buffer(format("error running command '{}': {}",
                                       m_buffer, e.what()));
                }
                catch (client_removed&) {}
                close(socket);
                Server::instance().remove_accepter(this);
                return;
            }
            if (c == 0) // end of initial command stream, go to interactive ui
            {
                EnvVarMap env_vars = read_map<String, String, MemoryDomain::EnvVars>(socket);
                std::unique_ptr<UserInterface> ui{new RemoteUI{socket}};
                ClientManager::instance().create_client(std::move(ui),
                                                        std::move(env_vars),
                                                        m_buffer);
                Server::instance().remove_accepter(this);
                return;
            }
            else
                m_buffer += c;

            FD_ZERO(&rfds);
            FD_SET(socket, &rfds);
        }
        while (select(socket+1, &rfds, nullptr, nullptr, &tv) == 1);
    }

    String    m_buffer;
    FDWatcher m_socket_watcher;
};

Server::Server(String session_name)
    : m_session{std::move(session_name)}
{
    int listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr = session_addr(m_session);

    make_directory(split_path(addr.sun_path).first);

    if (bind(listen_sock, (sockaddr*) &addr, sizeof(sockaddr_un)) == -1)
       throw runtime_error(format("unable to bind listen socket '{}'", addr.sun_path));

    if (listen(listen_sock, 4) == -1)
       throw runtime_error(format("unable to listen on socket '{}'", addr.sun_path));

    auto accepter = [this](FDWatcher& watcher, EventMode mode) {
        sockaddr_un client_addr;
        socklen_t   client_addr_len = sizeof(sockaddr_un);
        int sock = accept(watcher.fd(), (sockaddr*) &client_addr,
                          &client_addr_len);
        if (sock == -1)
            throw runtime_error("accept failed");
        fcntl(sock, F_SETFD, FD_CLOEXEC);

        m_accepters.emplace_back(new Accepter{sock});
    };
    m_listener.reset(new FDWatcher{listen_sock, accepter});
}

void Server::close_session()
{
    char socket_file[128];
    format_to(socket_file, "/tmp/kakoune/{}/{}", getlogin(), m_session);
    unlink(socket_file);
    m_listener->close_fd();
    m_listener.reset();
}

Server::~Server()
{
    if (m_listener)
        close_session();
}

void Server::remove_accepter(Accepter* accepter)
{
    auto it = find(m_accepters, accepter);
    kak_assert(it != m_accepters.end());
    m_accepters.erase(it);
}

}
