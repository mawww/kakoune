#include "remote.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "file.hh"
#include "id_map.hh"
#include "user_interface.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>


namespace Kakoune
{

enum class MessageType : char
{
    Unknown,
    Connect,
    Command,
    MenuShow,
    MenuSelect,
    MenuHide,
    InfoShow,
    InfoHide,
    Draw,
    DrawStatus,
    Refresh,
    SetOptions,
    Key
};

class MsgWriter
{
public:
    MsgWriter(int sock, MessageType type) : m_socket(sock)
    {
        write(type);
        write((uint32_t)0); // message size, to be patched on write
    }

    ~MsgWriter() noexcept(false)
    {
        uint32_t count = (uint32_t)m_stream.size();
        char* data = m_stream.data();
        *reinterpret_cast<uint32_t*>(data+1) = count;
        while (count > 0)
        {
            int res = ::write(m_socket, data, count);
            if (res <= 0)
                throw remote_error{res ? "peer disconnected"
                                       : format("socket write failed: {}", strerror(errno))};
            data += res;
            count -= res;
        }
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

    template<typename Val, MemoryDomain domain>
    void write(const IdMap<Val, domain>& map)
    {
        write<uint32_t>(map.size());
        for (auto& val : map)
        {
            write(val.key);
            write(val.value);
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
    Vector<char, MemoryDomain::Remote> m_stream;
    int m_socket;
};

class MsgReader
{
public:
    void read_available(int sock)
    {
        if (m_write_pos < header_size)
        {
            m_stream.resize(header_size);
            read_from_socket(sock, header_size - m_write_pos);
            if (m_write_pos == header_size)
                m_stream.resize(size());
        }
        else
            read_from_socket(sock, size() - m_write_pos);
    }

    bool ready() const
    {
        return m_write_pos >= header_size and m_write_pos == size();
    }

    uint32_t size() const
    {
        kak_assert(m_write_pos >= header_size); 
        return *reinterpret_cast<const uint32_t*>(m_stream.data()+1);
    }

    MessageType type() const
    {
        kak_assert(m_write_pos >= header_size); 
        return *reinterpret_cast<const MessageType*>(m_stream.data());
    }

    void read(char* buffer, size_t size)
    {
        if (m_read_pos + size > m_stream.size())
            throw remote_error{"tried to read after message end"};
        memcpy(buffer, m_stream.data() + m_read_pos, size);
        m_read_pos += size;
    }

    template<typename T>
    T read()
    {
        union U
        {
            T object;
            alignas(T) char data[sizeof(T)];
            U() {}
            ~U() { object.~T(); }
        } u;
        read(u.data, sizeof(T));
        return u.object;
    }

    template<typename T>
    Vector<T> read_vector()
    {
        uint32_t size = read<uint32_t>();
        Vector<T> res;
        res.reserve(size);
        while (size--)
            res.push_back(read<T>());
        return res;
    }

    template<typename Val, MemoryDomain domain>
    IdMap<Val, domain> read_idmap()
    {
        uint32_t size = read<uint32_t>();
        IdMap<Val, domain> res;
        res.reserve(size);
        while (size--)
        {
            auto key = read<String>();
            auto val = read<Val>();
            res.append({std::move(key), std::move(val)});
        }
        return res;
    }

    void reset()
    {
        m_stream.resize(0);
        m_write_pos = 0;
        m_read_pos = header_size;
    }

private:
    void read_from_socket(int sock, size_t size)
    {
        int res = ::read(sock, m_stream.data() + m_write_pos, size);
        if (res <= 0)
            throw remote_error{res ? "peer disconnected"
                                   : format("socket read failed: {}", strerror(errno))};
        m_write_pos += res;
    }

    static constexpr uint32_t header_size = sizeof(MessageType) + sizeof(uint32_t);
    Vector<char, MemoryDomain::Remote> m_stream;
    uint32_t m_write_pos = 0;
    uint32_t m_read_pos = header_size;
};

template<>
String MsgReader::read<String>()
{
    ByteCount length = read<ByteCount>();
    String res;
    if (length > 0)
    {
        res.force_size((int)length);
        read(&res[0_byte], (int)length);
    }
    return res;
}

template<>
Color MsgReader::read<Color>()
{
    Color res;
    res.color = read<Color::NamedColor>();
    if (res.color == Color::RGB)
    {
        res.r = read<unsigned char>();
        res.g = read<unsigned char>();
        res.b = read<unsigned char>();
    }
    return res;
}

template<>
DisplayAtom MsgReader::read<DisplayAtom>()
{
    DisplayAtom atom(read<String>());
    atom.face = read<Face>();
    return atom;
}

template<>
DisplayLine MsgReader::read<DisplayLine>()
{
    return DisplayLine(read_vector<DisplayAtom>());
}

template<>
DisplayBuffer MsgReader::read<DisplayBuffer>()
{
    DisplayBuffer db;
    db.lines() = read_vector<DisplayLine>();
    return db;
}


class RemoteUI : public UserInterface
{
public:
    RemoteUI(int socket, DisplayCoord dimensions);
    ~RemoteUI();

    void menu_show(ConstArrayView<DisplayLine> choices,
                   DisplayCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(StringView title, StringView content,
                   DisplayCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face,
              const Face& padding_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    void refresh(bool force) override;

    bool is_key_available() override;
    Key  get_key() override;
    DisplayCoord dimensions() override;

    void set_input_callback(InputCallback callback) override;

    void set_ui_options(const Options& options) override;

    void set_client(Client* client) { m_client = client; }
    Client* client() const { return m_client.get(); }

private:
    FDWatcher    m_socket_watcher;
    MsgReader    m_reader;
    DisplayCoord m_dimensions;
    InputCallback m_input_callback;

    SafePtr<Client> m_client;
    Optional<Key>   m_pending_key;
};


RemoteUI::RemoteUI(int socket, DisplayCoord dimensions)
    : m_socket_watcher(socket, [this](FDWatcher& watcher, EventMode mode) {
          const int sock = watcher.fd();
          bool disconnect = false;
          try
          {
              while (fd_readable(sock) and not m_reader.ready())
                  m_reader.read_available(sock);

              if (m_reader.ready() and not m_pending_key)
              {
                   if (m_reader.type() == MessageType::Key)
                   {
                       m_pending_key = m_reader.read<Key>();
                       m_reader.reset();
                   }
                   else
                       disconnect = true;
              }
          }
          catch (const remote_error& err)
          {
              write_to_debug_buffer(format("Error while reading remote message: {}", err.what()));
              disconnect = true;
          }

          if (disconnect)
              ClientManager::instance().remove_client(*m_client, false);
          else if (m_pending_key and m_input_callback)
              m_input_callback(mode);
      }),
      m_dimensions(dimensions)
{
    write_to_debug_buffer(format("remote client connected: {}", m_socket_watcher.fd()));
}

RemoteUI::~RemoteUI()
{
    write_to_debug_buffer(format("remote client disconnected: {}", m_socket_watcher.fd()));
    m_socket_watcher.close_fd();
}

void RemoteUI::menu_show(ConstArrayView<DisplayLine> choices,
                         DisplayCoord anchor, Face fg, Face bg,
                         MenuStyle style)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::MenuShow};
    msg.write(choices);
    msg.write(anchor);
    msg.write(fg);
    msg.write(bg);
    msg.write(style);
}

void RemoteUI::menu_select(int selected)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::MenuSelect};
    msg.write(selected);
}

void RemoteUI::menu_hide()
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::MenuHide};
}

void RemoteUI::info_show(StringView title, StringView content,
                         DisplayCoord anchor, Face face,
                         InfoStyle style)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::InfoShow};
    msg.write(title);
    msg.write(content);
    msg.write(anchor);
    msg.write(face);
    msg.write(style);
}

void RemoteUI::info_hide()
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::InfoHide};
}

void RemoteUI::draw(const DisplayBuffer& display_buffer,
                    const Face& default_face,
                    const Face& padding_face)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::Draw};
    msg.write(display_buffer);
    msg.write(default_face);
    msg.write(padding_face);
}

void RemoteUI::draw_status(const DisplayLine& status_line,
                           const DisplayLine& mode_line,
                           const Face& default_face)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::DrawStatus};
    msg.write(status_line);
    msg.write(mode_line);
    msg.write(default_face);
}

void RemoteUI::refresh(bool force)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::Refresh};
    msg.write(force);
}

void RemoteUI::set_ui_options(const Options& options)
{
    MsgWriter msg{m_socket_watcher.fd(), MessageType::SetOptions};
    msg.write(options);
}

bool RemoteUI::is_key_available()
{
    return (bool)m_pending_key;
}

Key RemoteUI::get_key()
{
    kak_assert(m_pending_key);
    auto key = *m_pending_key;
    m_pending_key.reset();
    if (key.modifiers == Key::Modifiers::Resize)
        m_dimensions = key.coord();
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

static sockaddr_un session_addr(StringView session)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if (find(session, '/')!= session.end())
        format_to(addr.sun_path, "/tmp/kakoune/{}", session);
    else
        format_to(addr.sun_path, "/tmp/kakoune/{}/{}", getpwuid(geteuid())->pw_name, session);
    return addr;
}

static int connect_to(StringView session)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr = session_addr(session);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) == -1)
        throw remote_error(format("connect to {} failed", addr.sun_path));
    return sock;
}

bool check_session(StringView session)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    auto close_sock = on_scope_end([sock]{ close(sock); });
    sockaddr_un addr = session_addr(session);
    return connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) != -1;
}

RemoteClient::RemoteClient(StringView session, std::unique_ptr<UserInterface>&& ui,
                           const EnvVarMap& env_vars, StringView init_command)
    : m_ui(std::move(ui))
{
    int sock = connect_to(session);

    {
        MsgWriter msg{sock, MessageType::Connect};
        msg.write(init_command);
        msg.write(m_ui->dimensions());
        msg.write(env_vars);
    }

    m_ui->set_input_callback([this](EventMode){ send_available_keys(); });

    MsgReader reader;
    m_socket_watcher.reset(new FDWatcher{sock, [this, reader](FDWatcher& watcher, EventMode) mutable {
        const int sock = watcher.fd();
        while (fd_readable(sock) and not reader.ready())
            reader.read_available(sock);

        if (not reader.ready())
            return;

        auto clear_reader = on_scope_end([&reader] { reader.reset(); });
        switch (reader.type())
        {
        case MessageType::MenuShow:
        {
            auto choices = reader.read_vector<DisplayLine>();
            auto anchor = reader.read<DisplayCoord>();
            auto fg = reader.read<Face>();
            auto bg = reader.read<Face>();
            auto style = reader.read<MenuStyle>();
            m_ui->menu_show(choices, anchor, fg, bg, style);
            break;
        }
        case MessageType::MenuSelect:
            m_ui->menu_select(reader.read<int>());
            break;
        case MessageType::MenuHide:
            m_ui->menu_hide();
            break;
        case MessageType::InfoShow:
        {
            auto title = reader.read<String>();
            auto content = reader.read<String>();
            auto anchor = reader.read<DisplayCoord>();
            auto face = reader.read<Face>();
            auto style = reader.read<InfoStyle>();
            m_ui->info_show(title, content, anchor, face, style);
            break;
        }
        case MessageType::InfoHide:
            m_ui->info_hide();
            break;
        case MessageType::Draw:
        {
            auto display_buffer = reader.read<DisplayBuffer>();
            auto default_face = reader.read<Face>();
            auto padding_face = reader.read<Face>();
            m_ui->draw(display_buffer, default_face, padding_face);
            break;
        }
        case MessageType::DrawStatus:
        {
            auto status_line = reader.read<DisplayLine>();
            auto mode_line = reader.read<DisplayLine>();
            auto default_face = reader.read<Face>();
            m_ui->draw_status(status_line, mode_line, default_face);
            break;
        }
        case MessageType::Refresh:
            m_ui->refresh(reader.read<bool>());
            break;
        case MessageType::SetOptions:
            m_ui->set_ui_options(reader.read_idmap<String, MemoryDomain::Options>());
            break;
        default:
            kak_assert(false);
        }
    }});
}

void RemoteClient::send_available_keys()
{
    while (m_ui->is_key_available())
    {
        MsgWriter msg(m_socket_watcher->fd(), MessageType::Key);
        msg.write(m_ui->get_key());
    }
}

void send_command(StringView session, StringView command)
{
    int sock = connect_to(session);
    auto close_sock = on_scope_end([sock]{ close(sock); });
    MsgWriter msg{sock, MessageType::Command};
    msg.write(command);
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
        const int sock = m_socket_watcher.fd();
        try
        {
            do
            {
                m_reader.read_available(sock);
            }
            while (fd_readable(sock) and not m_reader.ready());

            if (not m_reader.ready())
                return;

            switch (m_reader.type())
            {
            case MessageType::Connect:
            {
                auto init_command = m_reader.read<String>();
                auto dimensions = m_reader.read<DisplayCoord>();
                auto env_vars = m_reader.read_idmap<String, MemoryDomain::EnvVars>();
                RemoteUI* ui = new RemoteUI{sock, dimensions};
                if (auto* client = ClientManager::instance().create_client(
                                       std::unique_ptr<UserInterface>{ui},
                                       std::move(env_vars), init_command))
                    ui->set_client(client);

                Server::instance().remove_accepter(this);
                break;
            }
            case MessageType::Command:
            {
                auto command = m_reader.read<String>();
                if (not command.empty()) try
                {
                    Context context{Context::EmptyContextFlag{}};
                    CommandManager::instance().execute(command, context);
                }
                catch (const runtime_error& e)
                {
                    write_to_debug_buffer(format("error running command '{}': {}",
                                                 command, e.what()));
                }
                close(sock);
                Server::instance().remove_accepter(this);
                break;
            }
            default:
                write_to_debug_buffer("Invalid introduction message received");
                close(sock);
                Server::instance().remove_accepter(this);
            }
        }
        catch (const remote_error& err)
        {
            write_to_debug_buffer(format("accepting connection failed: {}", err.what()));
            close(sock);
            Server::instance().remove_accepter(this);
        }
    }

    FDWatcher m_socket_watcher;
    MsgReader m_reader;
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

bool Server::rename_session(StringView name)
{
    String old_socket_file = format("/tmp/kakoune/{}/{}", getpwuid(geteuid())->pw_name, m_session);
    String new_socket_file = format("/tmp/kakoune/{}/{}", getpwuid(geteuid())->pw_name, name);

    if (rename(old_socket_file.c_str(), new_socket_file.c_str()) != 0)
        return false;

    m_session = name.str();
    return true;
}

void Server::close_session(bool do_unlink)
{
    if (do_unlink)
    {
        String socket_file = format("/tmp/kakoune/{}/{}", getpwuid(geteuid())->pw_name, m_session);
        unlink(socket_file.c_str());
    }
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
