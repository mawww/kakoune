#include "remote.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "display_buffer.hh"
#include "event_manager.hh"
#include "field_writer.hh"
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

    // 9p messages
    Tversion = 100,
    Rversion,
    Tauth    = 102,
    Rauth,
    Tattach  = 104,
    Rattach,
    Terror   = 106,    // illegal
    Rerror,
    Tflush   = 108,
    Rflush,
    Twalk    = 110,
    Rwalk,
    Topen    = 112,
    Ropen,
    Tcreate  = 114,
    Rcreate,
    Tread    = 116,
    Rread,
    Twrite   = 118,
    Rwrite,
    Tclunk   = 120,
    Rclunk,
    Tremove  = 122,
    Rremove,
    Tstat    = 124,
    Rstat,
    Twstat   = 126,
    Rwstat,
};

typedef FieldWriter<uint32_t> KakouneFieldWriter;

class MsgWriter
{
public:
    MsgWriter(RemoteBuffer& buffer)
        : m_buffer{buffer}, m_start{buffer.size()}
    {
        uint32_t size{0};
        auto p = reinterpret_cast<const char*>(&size);
        m_buffer.insert(m_buffer.end(), p, p + sizeof(size)); // message size, to be patched on write
    }

    ~MsgWriter()
    {
        uint32_t count = uint32_t(m_buffer.size() - m_start);
        memcpy(m_buffer.data() + m_start, &count, sizeof(uint32_t));
    }

private:
    RemoteBuffer& m_buffer;
    decltype(m_buffer.size()) m_start;
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
        memcpy(&res, m_stream.data(), sizeof(uint32_t));
        return res;
    }

    void read(char* buffer, size_t size)
    {
        if (m_read_pos + size > m_stream.size())
            throw disconnected{"tried to read after message end"};
        memcpy(buffer, m_stream.data() + m_read_pos, size);
        m_read_pos += size;
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
        kak_assert(m_write_pos + size <= m_stream.size());
        int res = ::read(sock, m_stream.data() + m_write_pos, size);
        if (res <= 0)
            throw disconnected{format("socket read failed: {}", strerror(errno))};
        m_write_pos += res;
    }

    static constexpr uint32_t header_size = sizeof(uint32_t);
    RemoteBuffer m_stream;
    uint32_t m_write_pos = 0;
    uint32_t m_read_pos = header_size;
};

template<typename VariableSizeType>
class FieldReader
{
private:
    template<typename Size, typename T>
    struct Reader {
        static T read(MsgReader& reader)
        {
            static_assert(std::is_trivially_copyable<T>::value, "");
            T res;
            reader.read(reinterpret_cast<char*>(&res), sizeof(T));
            return res;
        }
    };

    template<typename Size, typename T, MemoryDomain domain>
    struct Reader<Size, Vector<T,domain>> {
        static Vector<T, domain> read(MsgReader& reader)
        {
            Size size = Reader<Size, Size>::read(reader);
            Vector<T,domain> res;
            res.reserve(size);
            while (size--)
                res.push_back(std::move(Reader<Size, T>::read(reader)));
            return res;
        }
    };

    template<typename Size, typename Key, typename Value, MemoryDomain domain>
    struct Reader<Size, HashMap<Key, Value, domain>> {
        static HashMap<Key, Value, domain> read(MsgReader& reader)
        {
            Size size = Reader<Size, Size>::read(reader);
            HashMap<Key, Value, domain> res;
            res.reserve(size);
            while (size--)
            {
                auto key = Reader<Size, Key>::read(reader);
                auto val = Reader<Size, Value>::read(reader);
                res.insert({std::move(key), std::move(val)});
            }
            return res;
        }
    };

    template<typename Size, typename T>
    struct Reader<Size, Optional<T>> {
        static Optional<T> read(MsgReader& reader)
        {
            if (not Reader<Size, bool>::read(reader))
                return {};
            return Reader<Size, T>::read(reader);
        }
    };

    template<typename Size>
    struct Reader<Size, String> {
        static String read(MsgReader& reader)
        {
            Size length = Reader<Size, Size>::read(reader);
            String res;
            if (length > 0)
            {
                res.force_size((int)length);
                reader.read(&res[0_byte], (int)length);
            }
            return res;
        }
    };

    template<typename Size>
    struct Reader<Size, Color> {
        static Color read(MsgReader& reader)
        {
            Color res;
            res.color = Reader<Size, Color::NamedColor>::read(reader);
            if (res.color == Color::RGB)
            {
                res.r = Reader<Size, unsigned char>::read(reader);
                res.g = Reader<Size, unsigned char>::read(reader);
                res.b = Reader<Size, unsigned char>::read(reader);
            }
            return res;
        }
    };

    template<typename Size>
    struct Reader<Size, DisplayAtom> {
        static DisplayAtom read(MsgReader& reader)
        {
            String content = Reader<Size, String>::read(reader);
            return {std::move(content), Reader<Size, Face>::read(reader)};
        }
    };

    template<typename Size>
    struct Reader<Size, DisplayLine> {
        static DisplayLine read(MsgReader& reader)
        {
            return {Reader<Size, Vector<DisplayAtom>>::read(reader)};
        }
    };

    template<typename Size>
    struct Reader<Size, DisplayBuffer> {
        static DisplayBuffer read(MsgReader& reader)
        {
            DisplayBuffer db;
            db.lines() = Reader<Size, Vector<DisplayLine>>::read(reader);
            return db;
        }
    };

public:
    FieldReader(MsgReader& reader)
        : m_reader(reader)
    {
    }

    template<typename T>
    T read()
    {
        return Reader<VariableSizeType,T>::read(m_reader);
    }

private:
    MsgReader& m_reader;
};

typedef FieldReader<uint32_t> KakouneFieldReader;
typedef FieldReader<uint16_t> NinePFieldReader;


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
        MsgWriter msg{m_send_buffer};
        KakouneFieldWriter fields{m_send_buffer};
        fields.write(type, std::forward<Args>(args)...);
        m_socket_watcher.events() |= FdEvents::Write;
    }

    FDWatcher     m_socket_watcher;
    MsgReader     m_reader;
    DisplayCoord  m_dimensions;
    OnKeyCallback m_on_key;
    RemoteBuffer  m_send_buffer;
};

static bool send_data(int fd, RemoteBuffer& buffer)
{
    while (not buffer.empty() and fd_writable(fd))
    {
      int res = ::write(fd, buffer.data(), buffer.size());
      if (res <= 0)
          throw disconnected{format("socket write failed: {}", strerror(errno))};
      buffer.erase(buffer.begin(), buffer.begin() + res);
    }
    return buffer.empty();
}

RemoteUI::RemoteUI(int socket, DisplayCoord dimensions)
    : m_socket_watcher(socket,  FdEvents::Read | FdEvents::Write,
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

                   KakouneFieldReader fields{m_reader};
                   auto type = fields.read<MessageType>();
                   if (type != MessageType::Key)
                   {
                       m_socket_watcher.close_fd();
                       return;
                   }

                   auto key = fields.read<Key>();
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

String session_directory()
{
    StringView xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir.empty())
        return format("{}/kakoune/{}", tmpdir(), get_user_name());
    else
        return format("{}/kakoune", xdg_runtime_dir);
}

void make_session_directory()
{
    StringView xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir.empty())
    {
        // set sticky bit on the shared kakoune directory
        make_directory(format("{}/kakoune", tmpdir()), 01777);
    }
    make_directory(session_directory(), 0711);
}

String session_path(StringView session)
{
    if (contains(session, '/'))
        throw runtime_error{"session names cannot have slashes"};
    return format("{}/{}", session_directory(), session);
}

static sockaddr_un session_addr(StringView session)
{
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, session_path(session).c_str());
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
                           Optional<BufferCoord> init_coord)
    : m_ui(std::move(ui))
{
    int sock = connect_to(session);

    {
        MsgWriter msg{m_send_buffer};
        KakouneFieldWriter fields{m_send_buffer};
        fields.write(MessageType::Connect, pid, name, init_command, init_coord, m_ui->dimensions(), env_vars);
    }

    m_ui->set_on_key([this](Key key){
        MsgWriter msg{m_send_buffer};
        KakouneFieldWriter fields{m_send_buffer};
        fields.write(MessageType::Key, key);
        m_socket_watcher->events() |= FdEvents::Write;
    });

    m_socket_watcher.reset(new FDWatcher{sock, FdEvents::Read | FdEvents::Write,
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
            KakouneFieldReader fields{reader};
            auto type = fields.read<MessageType>();
            switch (type)
            {
            case MessageType::MenuShow:
            {
                auto choices = fields.read<Vector<DisplayLine>>();
                auto anchor = fields.read<DisplayCoord>();
                auto fg = fields.read<Face>();
                auto bg = fields.read<Face>();
                auto style = fields.read<MenuStyle>();
                m_ui->menu_show(choices, anchor, fg, bg, style);
                break;
            }
            case MessageType::MenuSelect:
                m_ui->menu_select(fields.read<int>());
                break;
            case MessageType::MenuHide:
                m_ui->menu_hide();
                break;
            case MessageType::InfoShow:
            {
                auto title = fields.read<DisplayLine>();
                auto content = fields.read<DisplayLineList>();
                auto anchor = fields.read<DisplayCoord>();
                auto face = fields.read<Face>();
                auto style = fields.read<InfoStyle>();
                m_ui->info_show(title, content, anchor, face, style);
                break;
            }
            case MessageType::InfoHide:
                m_ui->info_hide();
                break;
            case MessageType::Draw:
            {
                auto display_buffer = fields.read<DisplayBuffer>();
                auto default_face = fields.read<Face>();
                auto padding_face = fields.read<Face>();
                m_ui->draw(display_buffer, default_face, padding_face);
                break;
            }
            case MessageType::DrawStatus:
            {
                auto status_line = fields.read<DisplayLine>();
                auto mode_line = fields.read<DisplayLine>();
                auto default_face = fields.read<Face>();
                m_ui->draw_status(status_line, mode_line, default_face);
                break;
            }
            case MessageType::SetCursor:
            {
                auto mode = fields.read<CursorMode>();
                auto coord = fields.read<DisplayCoord>();
                m_ui->set_cursor(mode, coord);
                break;
            }
            case MessageType::Refresh:
                m_ui->refresh(fields.read<bool>());
                break;
            case MessageType::SetOptions:
                m_ui->set_ui_options(fields.read<HashMap<String, String, MemoryDomain::Options>>());
                break;
            case MessageType::Exit:
                m_exit_status = fields.read<int>();
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
        MsgWriter msg{buffer};
        KakouneFieldWriter fields{buffer};
        fields.write(MessageType::Command, command);
    }
    write(sock, {buffer.data(), buffer.data() + buffer.size()});
}

class File {
public:
    typedef uint32_t Fid;

    enum class Type : uint8_t {
        DMDIR    = 0x80,
        DMAPPEND = 0x40,
        DMEXCL   = 0x20,
        DMTMP    = 0x04
    };
    friend constexpr bool with_bit_ops(Meta::Type<Type>) { return true; }

#pragma pack(push,1)
    struct Qid {
        Type     type;
        uint32_t version;
        uint64_t path;
    };
#pragma pack(pop)

    static_assert(sizeof(Qid) == 13, "compiler has added padding to Qid");

    File(Type type, const Vector<String>& path)
      : m_type(type), m_path(path)
    {}
    virtual ~File() {}
    virtual Vector<RemoteBuffer> contents() const = 0;
    virtual std::unique_ptr<File> walk(const String& name) const = 0;

    Type type() const { return m_type; }
    const Vector<String>& path() const { return m_path; }

    String full_path() const
    {
        if (m_path.empty())
            return String{"/"};
        else
            return join(m_path, '/', false);
    }

    Qid qid() const
    {
        String path = full_path();
        uint64_t path_hash = hash_data(path.data(), size_t(int(path.length())));
        return { m_type, 0, path_hash };
    }

    uint32_t mode() const
    {
        uint32_t mode = uint32_t(m_type) << 24;
        if (m_type & Type::DMDIR)
            mode |= 0755;
        else
            mode |= 0644;
        return mode;
    }

    uint64_t length() const
    {
        ByteCount length{0};
        if (not (m_type & Type::DMDIR))
        {
            auto data = contents();
            for (auto& s : data)
                length += s.size();
        }
        return uint64_t(int(length));
    }

    String basename() const
    {
        if (m_path.empty())
            return "/";
        else
            return *m_path.rbegin();
    }

    RemoteBuffer stat() const
    {
        RemoteBuffer stat_data;
        {
            NinePFieldWriter fields{stat_data};
            fields.write<uint16_t>(0);  // ???
            fields.write<uint16_t>(0);  // type, "for kernel use"
            fields.write<uint32_t>(0);  // dev, "for kernel use"
            fields.write(qid());
            fields.write<uint32_t>(mode());
            fields.write<uint32_t>(0); // atime
            fields.write<uint32_t>(0); // mtime
            fields.write<uint64_t>(length());
            fields.write(basename());
            fields.write(get_user_name());
            fields.write("group");
            fields.write(get_user_name());
        }

        RemoteBuffer result;
        {
            NinePFieldWriter result_fields{result};
            result_fields.write<uint16_t>(int(stat_data.size()));
            result_fields.write(stat_data.data(), int(stat_data.size()));
        }
        return result;
    }

private:
    Type m_type;
    Vector<String> m_path;
};

class Root : public File {
public:
    Root()
      : File(Type::DMDIR, Vector<String>())
    {}

    virtual Vector<RemoteBuffer> contents() const
    {
        return {};
    }

    virtual std::unique_ptr<File> walk(const String& name) const
    {
        return {};
    }
};

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
        : m_socket_watcher(socket, FdEvents::Read,
                           [this](FDWatcher&, FdEvents events, EventMode mode) {
                               handle_events(events, mode);
                           })
    {}

private:
    class FidState {
    public:
        explicit FidState(std::shared_ptr<File> file)
            : m_file{file}
        {
        }

        std::shared_ptr<File> file() const { return m_file; }
        bool is_open() const { return m_is_open; }

        bool open()
        {
            if (m_is_open)
                return false;
            m_open_contents = m_file->contents();
            m_is_open = true;
            return true;
        }

        RemoteBuffer read(uint64_t offset, uint32_t count)
        {
            if (m_file->type() & File::Type::DMDIR)
            {
                uint64_t o = 0;
                auto it = m_open_contents.begin();
                auto end = m_open_contents.end();
                for (; o < offset && it != end; ++it)
                    o += it->size();
                if (o == offset) {
                    RemoteBuffer res;
                    while (it != end and it->size() < count) {
                        res.insert(res.end(), it->begin(), it->end());
                        count -= it->size();
                        ++it;
                    }
                    return res;
                } else {
                    // Bad read offset, should error
                    return RemoteBuffer{};
                }
            }
            else
            {
                // Normal file
                return RemoteBuffer{};
            }
        }

    private:
        std::shared_ptr<File> m_file;
        bool m_is_open = false;
        Vector<RemoteBuffer> m_open_contents;
    };

    void handle_events(FdEvents events, EventMode mode)
    {
        const int sock = m_socket_watcher.fd();
        try
        {
            if (events & FdEvents::Write and send_data(sock, m_send_buffer))
                m_socket_watcher.events() &= ~FdEvents::Write;

            while (events & FdEvents::Read and not m_reader.ready() and fd_readable(sock))
                m_reader.read_available(sock);

            if (mode != EventMode::Normal or not m_reader.ready())
                return;

            KakouneFieldReader fields{m_reader};
            auto type = fields.read<MessageType>();
            switch (type)
            {
            case MessageType::Connect:
            {
                auto pid = fields.read<int>();
                auto name = fields.read<String>();
                auto init_cmds = fields.read<String>();
                auto init_coord = fields.read<Optional<BufferCoord>>();
                auto dimensions = fields.read<DisplayCoord>();
                auto env_vars = fields.read<HashMap<String, String, MemoryDomain::EnvVars>>();
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
                auto command = fields.read<String>();
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
            case MessageType::Tattach:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto fid = fields.read<File::Fid>();
                auto afid = fields.read<File::Fid>();
                auto uname = fields.read<String>();
                auto aname = fields.read<String>();
                m_reader.reset();
                std::shared_ptr<File> file{ new Root() };
                m_fids.insert({ fid, FidState{ file } });
                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Rattach);
                    fields.write(tag);
                    fields.write(file->qid());
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            case MessageType::Tauth:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                m_reader.reset();
                error(tag, "Auth isn't supported");
                break;
            }
            case MessageType::Tclunk:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto fid = fields.read<File::Fid>();
                m_reader.reset();
                m_fids.erase(fid);
                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Rclunk);
                    fields.write(tag);
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            case MessageType::Topen:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto fid = fields.read<File::Fid>();
                auto mode = fields.read<uint8_t>();
                m_reader.reset();
                auto it = m_fids.find(fid);
                if (it == m_fids.end())
                {
                    error(tag, "Unknown FID");
                    return;
                }
                if (not it->value.open())
                {
                    error(tag, "Failed to open file");
                    return;
                }
                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Ropen);
                    fields.write(tag);
                    fields.write(it->value.file()->qid());
                    fields.write<uint32_t>(0);
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            case MessageType::Tread:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto fid = fields.read<File::Fid>();
                auto offset = fields.read<uint64_t>();
                auto count = fields.read<uint32_t>();
                m_reader.reset();

                auto it = m_fids.find(fid);
                if (it == m_fids.end())
                {
                    error(tag, "Unknown FID");
                    return;
                }
                if (not it->value.is_open())
                {
                    error(tag, "File is not open");
                    return;
                }

                auto data = it->value.read(offset, count);

                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Rread);
                    fields.write(tag);
                    fields.write<uint32_t>(int(data.size()));
                    fields.write(data.data(), data.size());
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            case MessageType::Tstat:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto fid = fields.read<File::Fid>();
                m_reader.reset();
                auto it = m_fids.find(fid);
                if (it == m_fids.end())
                {
                    error(tag, "Unknown FID");
                    return;
                }
                auto stat = it->value.file()->stat();
                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Rstat);
                    fields.write(tag);
                    fields.write<uint16_t>(uint16_t(stat.size()));
                    fields.write(stat.data(), stat.size());
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            case MessageType::Tversion:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto msize = fields.read<uint32_t>();
                auto version = fields.read<String>();
                m_reader.reset();
                const char* reply_version = version == "9P2000" ? "9P2000" : "unknown";
                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Rversion);
                    fields.write(tag);
                    fields.write(msize);
                    fields.write(reply_version);
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            case MessageType::Twalk:
            {
                NinePFieldReader fields{m_reader};
                auto tag = fields.read<uint16_t>();
                auto fid = fields.read<uint32_t>();
                auto newfid = fields.read<uint32_t>();
                auto nwnames = fields.read<Vector<String>>();
                m_reader.reset();

                auto file_it = m_fids.find(fid);
                if (file_it == m_fids.end())
                {
                    error(tag, "Unknown FID");
                    return;
                }
                auto file = file_it->value.file();

                if (m_fids.find(newfid) != m_fids.end())
                {
                    error(tag, "New FID already exists");
                    return;
                }

                Vector<File::Qid> qids;
                for (uint16_t i = 0; i < nwnames.size(); i++) {
                    file = file->walk(nwnames[i]);
                    if (not file) {
                        error(tag, "Not found");
                        return;
                    }
                    qids.push_back(file->qid());
                }
                m_fids.insert({ newfid, FidState{ file } });

                {
                    MsgWriter msg{m_send_buffer};
                    NinePFieldWriter fields{m_send_buffer};
                    fields.write(MessageType::Rwalk);
                    fields.write(tag);
                    fields.write(qids);
                }
                m_socket_watcher.events() |= FdEvents::Write;
                break;
            }
            default:
                write_to_debug_buffer(format("invalid introduction message received (type {})", int(type)));
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

    void error(uint16_t tag, StringView message)
    {
        MsgWriter msg{m_send_buffer};
        NinePFieldWriter fields{m_send_buffer};
        fields.write(MessageType::Rerror);
        fields.write(tag);
        fields.write(message);
        m_socket_watcher.events() |= FdEvents::Write;
    }

    FDWatcher m_socket_watcher;
    MsgReader m_reader;
    RemoteBuffer m_send_buffer;
    HashMap<File::Fid, FidState> m_fids;
};

Server::Server(String session_name)
    : m_session{std::move(session_name)}
{
    if (not all_of(m_session, is_identifier))
        throw runtime_error{format("invalid session name: '{}'", session_name)};

    int listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr = session_addr(m_session);

    make_session_directory();

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
    m_listener.reset(new FDWatcher{listen_sock, FdEvents::Read, accepter});
}

bool Server::rename_session(StringView name)
{
    if (not all_of(name, is_identifier))
        throw runtime_error{format("invalid session name: '{}'", name)};

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
