#include "remote.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "file.hh"
#include "hash_map.hh"
#include "optional.hh"
#include "user_interface.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>


namespace Kakoune
{

enum class MessageType : uint8_t
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
    SetCursor,
    Refresh,
    SetOptions,
    Exit,
    Key,
};

class MsgWriter
{
public:
    MsgWriter(RemoteBuffer& buffer, MessageType type)
        : m_buffer{buffer}, m_start{(uint32_t)buffer.size()}
    {
        write_field(type);
        write_field((uint32_t)0); // message size, to be patched on write
    }

    ~MsgWriter()
    {
        uint32_t count = (uint32_t)m_buffer.size() - m_start;
        memcpy(m_buffer.data() + m_start + sizeof(MessageType), &count, sizeof(uint32_t));
    }

    template<typename ...Args>
    void write(Args&&... args)
    {
        (write_field(std::forward<Args>(args)), ...);
    }

private:
    void write_raw(const char* val, size_t size)
    {
        m_buffer.insert(m_buffer.end(), val, val + size);
    }

    template<typename T>
    void write_field(const T& val)
    {
        static_assert(std::is_trivially_copyable<T>::value, "");
        write_raw((const char*)&val, sizeof(val));
    }

    void write_field(StringView str)
    {
        write_field(str.length());
        write_raw(str.data(), (int)str.length());
    };

    void write_field(const String& str)
    {
        write_field(StringView{str});
    }

    template<typename T>
    void write_field(ConstArrayView<T> view)
    {
        write_field<uint32_t>(view.size());
        for (auto& val : view)
            write_field(val);
    }

    template<typename T, MemoryDomain domain>
    void write_field(const Vector<T, domain>& vec)
    {
        write_field(ConstArrayView<T>(vec));
    }

    template<typename Key, typename Val, MemoryDomain domain>
    void write_field(const HashMap<Key, Val, domain>& map)
    {
        write_field<uint32_t>(map.size());
        for (auto& val : map)
        {
            write_field(val.key);
            write_field(val.value);
        }
    }

    template<typename T>
    void write_field(const Optional<T>& val)
    {
        write_field((bool)val);
        if (val)
            write_field(*val);
    }

    void write_field(Color color)
    {
        write_field(color.color);
        if (color.isRGB())
        {
            write_field(color.r);
            write_field(color.g);
            write_field(color.b);
        }
    }

    void write_field(const DisplayAtom& atom)
    {
        write_field(atom.content());
        write_field(atom.face);
    }

    void write_field(const DisplayLine& line)
    {
        write_field(line.atoms());
    }

    void write_field(const DisplayBuffer& display_buffer)
    {
        write_field(display_buffer.lines());
    }

private:
    RemoteBuffer& m_buffer;
    uint32_t m_start;
};

class MsgReader
{
private:
    template<typename T>
    struct Reader {
        static T read(MsgReader& reader)
        {
            static_assert(std::is_trivially_copyable<T>::value, "");
            T res;
            reader.read(reinterpret_cast<char*>(&res), sizeof(T));
            return res;
        }
    };

    template<typename T, MemoryDomain domain>
    struct Reader<Vector<T,domain>> {
        static Vector<T, domain> read(MsgReader& reader)
        {
            uint32_t size = Reader<uint32_t>::read(reader);
            Vector<T,domain> res;
            res.reserve(size);
            while (size--)
                res.push_back(std::move(Reader<T>::read(reader)));
            return res;
        }
    };

    template<typename Key, typename Value, MemoryDomain domain>
    struct Reader<HashMap<Key, Value, domain>> {
        static HashMap<Key, Value, domain> read(MsgReader& reader)
        {
            uint32_t size = Reader<uint32_t>::read(reader);
            HashMap<Key, Value, domain> res;
            res.reserve(size);
            while (size--)
            {
                auto key = Reader<Key>::read(reader);
                auto val = Reader<Value>::read(reader);
                res.insert({std::move(key), std::move(val)});
            }
            return res;
        }
    };

    template<typename T>
    struct Reader<Optional<T>> {
        static Optional<T> read(MsgReader& reader)
        {
            if (not Reader<bool>::read(reader))
                return {};
            return Reader<T>::read(reader);
        }
    };

public:
    void read_available(int sock)
    {
        if (m_write_pos < header_size)
        {
            m_stream.resize(header_size);
            read_from_socket(sock, header_size - m_write_pos);
            if (m_write_pos == header_size)
            {
                if (size() < header_size)
                    throw disconnected{"invalid message received"};
                m_stream.resize(size());
            }
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
        uint32_t res;
        memcpy(&res, m_stream.data() + sizeof(MessageType), sizeof(uint32_t));
        return res;
    }

    MessageType type() const
    {
        kak_assert(m_write_pos >= header_size);
        return *reinterpret_cast<const MessageType*>(m_stream.data());
    }

    void read(char* buffer, size_t size)
    {
        if (m_read_pos + size > m_stream.size())
            throw disconnected{"tried to read after message end"};
        memcpy(buffer, m_stream.data() + m_read_pos, size);
        m_read_pos += size;
    }

    template<typename T>
    T read()
    {
        return Reader<T>::read(*this);
    }

    Optional<int> ancillary_fd()
    {
        auto res = m_ancillary_fd;
        m_ancillary_fd.reset();
        return res;
    }

    ~MsgReader()
    {
        m_ancillary_fd.map(close);
    }

    void reset()
    {
        m_stream.resize(0);
        m_write_pos = 0;
        m_read_pos = header_size;
        m_ancillary_fd.map(close);
    }

private:
    void read_from_socket(int sock, size_t size)
    {
        kak_assert(m_write_pos + size <= m_stream.size());
        iovec io{m_stream.data() + m_write_pos, size};
        alignas(cmsghdr) char fdbuf[CMSG_SPACE(sizeof(int))];

        msghdr msg{};
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_control = fdbuf;
        msg.msg_controllen = sizeof(fdbuf);

        int res = recvmsg(sock, &msg, 0);
        if (res <= 0)
            throw disconnected{format("socket read failed: {}", strerror(errno))};

        m_write_pos += res;

        if (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS && cmsg->cmsg_len == CMSG_LEN(sizeof(int)))
        {
            m_ancillary_fd.map(close);
            memcpy(&m_ancillary_fd.emplace(), CMSG_DATA(cmsg), sizeof(int));
            fcntl(*m_ancillary_fd, F_SETFD, FD_CLOEXEC);
        }
    }

    static constexpr uint32_t header_size = sizeof(MessageType) + sizeof(uint32_t);
    Vector<char, MemoryDomain::Remote> m_stream;
    Optional<int> m_ancillary_fd;
    uint32_t m_write_pos = 0;
    uint32_t m_read_pos = header_size;
};

template<>
struct MsgReader::Reader<String> {
    static String read(MsgReader& reader)
    {
        ByteCount length = Reader<ByteCount>::read(reader);
        String res;
        if (length > 0)
        {
            res.force_size((int)length);
            reader.read(&res[0_byte], (int)length);
        }
        return res;
    }
};

template<>
struct MsgReader::Reader<Color> {
    static Color read(MsgReader& reader)
    {
        Color res;
        res.color = Reader<Color::NamedColor>::read(reader);
        if (res.isRGB())
        {
            res.r = Reader<unsigned char>::read(reader);
            res.g = Reader<unsigned char>::read(reader);
            res.b = Reader<unsigned char>::read(reader);
        }
        return res;
    }
};

template<>
struct MsgReader::Reader<DisplayAtom> {
    static DisplayAtom read(MsgReader& reader)
    {
        String content = Reader<String>::read(reader);
        return {std::move(content), Reader<Face>::read(reader)};
    }
};

template<>
struct MsgReader::Reader<DisplayLine> {
    static DisplayLine read(MsgReader& reader)
    {
        return {Reader<Vector<DisplayAtom>>::read(reader)};
    }
};

template<>
struct MsgReader::Reader<DisplayBuffer> {
    static DisplayBuffer read(MsgReader& reader)
    {
        DisplayBuffer db;
        db.lines() = Reader<Vector<DisplayLine>>::read(reader);
        return db;
    }
};


class RemoteUI : public UserInterface
{
public:
    RemoteUI(int socket, DisplayCoord dimensions);
    ~RemoteUI() override;

    bool is_ok() const override { return m_socket_watcher.fd() != -1; }
    void menu_show(ConstArrayView<DisplayLine> choices,
                   DisplayCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const DisplayLine& title, const DisplayLineList& content,
                   DisplayCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face,
              const Face& padding_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    void set_cursor(CursorMode mode, DisplayCoord coord) override;

    void refresh(bool force) override;

    DisplayCoord dimensions() override { return m_dimensions; }

    void set_on_key(OnKeyCallback callback) override
    { m_on_key = std::move(callback); }

    void set_ui_options(const Options& options) override;

    void exit(int status);

private:
    template<typename ...Args>
    void send_message(MessageType type, Args&&... args)
    {
        MsgWriter msg{m_send_buffer, type};
        msg.write(std::forward<Args>(args)...);
        m_socket_watcher.events() |= FdEvents::Write;
    }

    FDWatcher     m_socket_watcher;
    MsgReader     m_reader;
    DisplayCoord  m_dimensions;
    OnKeyCallback m_on_key;
    RemoteBuffer  m_send_buffer;
};

static bool send_data(int fd, RemoteBuffer& buffer, Optional<int> ancillary_fd = {})
{
    while (not buffer.empty() and fd_writable(fd))
    {
        iovec io{buffer.data(), buffer.size()};
        alignas(cmsghdr) char fdbuf[CMSG_SPACE(sizeof(int))];

        msghdr msg{};
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        if (ancillary_fd)
        {
            msg.msg_control = fdbuf;
            msg.msg_controllen = sizeof(fdbuf);

            cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            memcpy(CMSG_DATA(cmsg), &*ancillary_fd, sizeof(int));
        }

        int res = sendmsg(fd, &msg, 0);
        if (res <= 0)
              throw disconnected{format("socket write failed: {}", strerror(errno))};
         buffer.erase(buffer.begin(), buffer.begin() + res);
    }
    return buffer.empty();
}

RemoteUI::RemoteUI(int socket, DisplayCoord dimensions)
    : m_socket_watcher(socket,  FdEvents::Read | FdEvents::Write, EventMode::Urgent,
                       [this](FDWatcher& watcher, FdEvents events, EventMode) {
          const int sock = watcher.fd();
          try
          {
              if (events & FdEvents::Write and send_data(sock, m_send_buffer))
                  m_socket_watcher.events() &= ~FdEvents::Write;

              while (events & FdEvents::Read and fd_readable(sock))
              {
                  m_reader.read_available(sock);

                  if (not m_reader.ready())
                      continue;

                   if (m_reader.type() != MessageType::Key)
                   {
                       m_socket_watcher.close_fd();
                       return;
                   }

                   auto key = m_reader.read<Key>();
                   m_reader.reset();
                   if (key.modifiers == Key::Modifiers::Resize)
                       m_dimensions = key.coord();
                   m_on_key(key);
              }
          }
          catch (const disconnected& err)
          {
              write_to_debug_buffer(format("Error while transfering remote messages: {}", err.what()));
              m_socket_watcher.close_fd();
          }
      }),
      m_dimensions(dimensions)
{
    write_to_debug_buffer(format("remote client connected: {}", m_socket_watcher.fd()));
}

RemoteUI::~RemoteUI()
{
    // Try to send the remaining data if possible, as it might contain the desired exit status
    try
    {
        if (m_socket_watcher.fd() != -1)
            send_data(m_socket_watcher.fd(), m_send_buffer);
    }
    catch (disconnected&)
    {
    }

    write_to_debug_buffer(format("remote client disconnected: {}", m_socket_watcher.fd()));
    m_socket_watcher.close_fd();
}

void RemoteUI::menu_show(ConstArrayView<DisplayLine> choices,
                         DisplayCoord anchor, Face fg, Face bg,
                         MenuStyle style)
{
    send_message(MessageType::MenuShow, choices, anchor, fg, bg, style);
}

void RemoteUI::menu_select(int selected)
{
    send_message(MessageType::MenuSelect, selected);
}

void RemoteUI::menu_hide()
{
    send_message(MessageType::MenuHide);
}

void RemoteUI::info_show(const DisplayLine& title, const DisplayLineList& content,
                         DisplayCoord anchor, Face face,
                         InfoStyle style)
{
    send_message(MessageType::InfoShow, title, content, anchor, face, style);
}

void RemoteUI::info_hide()
{
    send_message(MessageType::InfoHide);
}

void RemoteUI::draw(const DisplayBuffer& display_buffer,
                    const Face& default_face,
                    const Face& padding_face)
{
    send_message(MessageType::Draw, display_buffer, default_face, padding_face);
}

void RemoteUI::draw_status(const DisplayLine& status_line,
                           const DisplayLine& mode_line,
                           const Face& default_face)
{
    send_message(MessageType::DrawStatus, status_line, mode_line, default_face);
}

void RemoteUI::set_cursor(CursorMode mode, DisplayCoord coord)
{
    send_message(MessageType::SetCursor, mode, coord);
}

void RemoteUI::refresh(bool force)
{
    send_message(MessageType::Refresh, force);
}

void RemoteUI::set_ui_options(const Options& options)
{
    send_message(MessageType::SetOptions, options);
}

void RemoteUI::exit(int status)
{
    send_message(MessageType::Exit, status);
}

String get_user_name()
{
    auto pw = getpwuid(geteuid());
    if (pw)
      return pw->pw_name;
    return getenv("USER");
}

const String& session_directory()
{
    static String session_dir = [] {
        StringView xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (not xdg_runtime_dir.empty())
        {
            if (struct stat st; stat(xdg_runtime_dir.zstr(), &st) == 0 && st.st_uid == geteuid())
                return format("{}/kakoune", xdg_runtime_dir);
            else
                write_to_debug_buffer("XDG_RUNTIME_DIR does not exist or not owned by current user, using tmpdir");
        }
        return format("{}/kakoune-{}", tmpdir(), get_user_name());
    }();
    return session_dir;
}

String session_path(StringView session)
{
    if (not all_of(session, is_identifier))
        throw runtime_error{format("invalid session name: '{}'", session)};
    return format("{}/{}", session_directory(), session);
}

static sockaddr_un session_addr(StringView session)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    String path = session_path(session);
    if (path.length() + 1 > sizeof addr.sun_path)
        throw runtime_error{format("socket path too long: '{}'", path)};
    strcpy(addr.sun_path, path.c_str());
    return addr;
}

static int connect_to(StringView session)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr = session_addr(session);
    if (connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) == -1)
        throw disconnected(format("connect to {} failed", addr.sun_path));
    return sock;
}

bool check_session(StringView session)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    auto close_sock = on_scope_end([sock]{ close(sock); });
    sockaddr_un addr = session_addr(session);
    return connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) != -1;
}

RemoteClient::RemoteClient(StringView session, StringView name, std::unique_ptr<UserInterface>&& ui,
                           int pid, const EnvVarMap& env_vars, StringView init_command,
                           Optional<BufferCoord> init_coord, Optional<int> stdin_fd)
    : m_ui(std::move(ui))
{
    int sock = connect_to(session);

    {
        MsgWriter msg{m_send_buffer, MessageType::Connect};
        msg.write(pid, name, init_command, init_coord, m_ui->dimensions(), env_vars);
    }
    send_data(sock, m_send_buffer, stdin_fd);

    m_ui->set_on_key([this](Key key){
        MsgWriter msg(m_send_buffer, MessageType::Key);
        msg.write(key);
        m_socket_watcher->events() |= FdEvents::Write;
     });

    m_socket_watcher.reset(new FDWatcher{sock, FdEvents::Read | FdEvents::Write, EventMode::Urgent,
                           [this, reader = MsgReader{}](FDWatcher& watcher, FdEvents events, EventMode) mutable {
        const int sock = watcher.fd();
        if (events & FdEvents::Write and send_data(sock, m_send_buffer))
            watcher.events() &= ~FdEvents::Write;

        while (events & FdEvents::Read and
               not reader.ready() and fd_readable(sock))
        {
            reader.read_available(sock);

            if (not reader.ready())
                continue;

            auto clear_reader = on_scope_end([&reader] { reader.reset(); });
            switch (reader.type())
            {
            case MessageType::MenuShow:
            {
                auto choices = reader.read<Vector<DisplayLine>>();
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
                auto title = reader.read<DisplayLine>();
                auto content = reader.read<DisplayLineList>();
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
            case MessageType::SetCursor:
            {
                auto mode = reader.read<CursorMode>();
                auto coord = reader.read<DisplayCoord>();
                m_ui->set_cursor(mode, coord);
                break;
            }
            case MessageType::Refresh:
                m_ui->refresh(reader.read<bool>());
                break;
            case MessageType::SetOptions:
                m_ui->set_ui_options(reader.read<HashMap<String, String, MemoryDomain::Options>>());
                break;
            case MessageType::Exit:
                m_exit_status = reader.read<int>();
                watcher.close_fd();
                return;
            default:
                kak_assert(false);
            }
        }
    }});
}

bool RemoteClient::is_ui_ok() const
{
    return m_ui->is_ok();
}

void send_command(StringView session, StringView command)
{
    int sock = connect_to(session);
    auto close_sock = on_scope_end([sock]{ close(sock); });
    RemoteBuffer buffer;
    {
        MsgWriter msg{buffer, MessageType::Command};
        msg.write(command);
    }
    write(sock, {buffer.data(), buffer.data() + buffer.size()});
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
        : m_socket_watcher(socket, FdEvents::Read, EventMode::Urgent,
                           [this](FDWatcher&, FdEvents, EventMode mode) {
                               handle_available_input(mode);
                           })
    {}

private:
    void handle_available_input(EventMode mode)
    {
        const int sock = m_socket_watcher.fd();
        try
        {
            while (not m_reader.ready() and fd_readable(sock))
                m_reader.read_available(sock);

            if (mode != EventMode::Normal or not m_reader.ready())
                return;

            switch (m_reader.type())
            {
            case MessageType::Connect:
            {
                auto pid = m_reader.read<int>();
                auto name = m_reader.read<String>();
                auto init_cmds = m_reader.read<String>();
                auto init_coord = m_reader.read<Optional<BufferCoord>>();
                auto dimensions = m_reader.read<DisplayCoord>();
                auto env_vars = m_reader.read<HashMap<String, String, MemoryDomain::EnvVars>>();

                if (auto stdin_fd = m_reader.ancillary_fd())
                    create_fifo_buffer(generate_buffer_name("*stdin-{}*"), *stdin_fd, Buffer::Flags::None);

                auto* ui = new RemoteUI{sock, dimensions};
                ClientManager::instance().create_client(
                    std::unique_ptr<UserInterface>(ui), pid, std::move(name),
                    std::move(env_vars), init_cmds, init_coord,
                    [ui](int status) { ui->exit(status); });

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
                write_to_debug_buffer("invalid introduction message received");
                close(sock);
                Server::instance().remove_accepter(this);
            }
        }
        catch (const disconnected& err)
        {
            write_to_debug_buffer(format("accepting connection failed: {}", err.what()));
            close(sock);
            Server::instance().remove_accepter(this);
        }
    }

    FDWatcher m_socket_watcher;
    MsgReader m_reader;
};

Server::Server(String session_name, bool is_daemon)
    : m_session{std::move(session_name)}, m_is_daemon{is_daemon}
{
    int listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr = session_addr(m_session);

    make_directory(session_directory(), 0711);

    // Do not give any access to the socket to other users by default
    auto old_mask = umask(0077);
    auto restore_mask = on_scope_end([old_mask]() { umask(old_mask); });

    if (bind(listen_sock, (sockaddr*) &addr, sizeof(sockaddr_un)) == -1)
       throw runtime_error(format("unable to bind listen socket '{}': {}",
                                  addr.sun_path, strerror(errno)));

    if (listen(listen_sock, 4) == -1)
       throw runtime_error(format("unable to listen on socket '{}': {}",
                                  addr.sun_path, strerror(errno)));

    auto accepter = [this](FDWatcher& watcher, FdEvents, EventMode) {
        sockaddr_un client_addr;
        socklen_t   client_addr_len = sizeof(sockaddr_un);
        int sock = accept(watcher.fd(), (sockaddr*) &client_addr,
                          &client_addr_len);
        if (sock == -1)
            throw runtime_error("accept failed");
        fcntl(sock, F_SETFD, FD_CLOEXEC);

        m_accepters.emplace_back(new Accepter{sock});
    };
    m_listener.reset(new FDWatcher{listen_sock, FdEvents::Read, EventMode::Urgent, accepter});
}

bool Server::rename_session(StringView name)
{
    String old_socket_file = session_path(m_session);
    String new_socket_file = session_path(name);

    if (file_exists(new_socket_file))
        return false;

    if (rename(old_socket_file.c_str(), new_socket_file.c_str()) != 0)
        return false;

    m_session = name.str();
    return true;
}

void Server::close_session(bool do_unlink)
{
    if (do_unlink)
    {
        String socket_file = session_path(m_session);
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
