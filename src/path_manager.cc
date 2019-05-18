#include "path_manager.hh"

#include "client_manager.hh"
#include "field_writer.hh"
#include "remote.hh"

namespace Kakoune
{

class GlobType
{
public:
    static GlobType* resolve(StringView name);

    virtual bool matches(StringView name, StringView text) const = 0;
    virtual Vector<String> expand(StringView name) const = 0;
};

struct LiteralGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        return name == text;
    }

    Vector<String> expand(StringView name) const
    {
        Vector<String> res;
        res.push_back(String{name});
        return res;
    }
} literal_glob_type{};

struct ClientNameGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        auto it = find_if(ClientManager::instance(), [&text](auto& client) { return client->context().name() == text; });
        return it != ClientManager::instance().end();
    }

    Vector<String> expand(StringView name) const
    {
        Vector<String> res;
        for (auto& client : ClientManager::instance())
            res.push_back(client->context().name());
        return res;
    }
} client_name_glob_type{};

GlobType* GlobType::resolve(StringView name)
{
    if (name == "$client_name")
        return &client_name_glob_type;
    return &literal_glob_type;
}

class Glob
{
public:
    Glob(StringView name)
        : m_name{name}
    {
    }

    bool matches(StringView text) const
    {
        return GlobType::resolve(m_name)->matches(m_name, text);
    }

    Vector<String> expand() const
    {
        return GlobType::resolve(m_name)->expand(m_name);
    }

private:
    String m_name;
};

class FileType
{
public:
    virtual RemoteBuffer read(const Vector<String>& path) const = 0;
};

// File

File::File(Vector<String> path, FileType* type)
    : m_path{path}, m_type{type}
{
}

File::Type File::type() const
{
    if (not m_type)
        return Type::DMDIR;
    else
        return Type(0);
}

const Vector<String>& File::path() const
{
    return m_path;
}

String File::fullname() const
{
    if (m_path.empty())
        return "/";
    return join(m_path, '/', false);
}

File::Qid File::qid() const
{
    String data = fullname();
    uint64_t path_hash = hash_data(data.data(), size_t(int(data.length())));
    return { type(), 0, path_hash };
}

uint32_t File::mode() const
{
    uint32_t mode = uint32_t(type()) << 24;
    if (type() & Type::DMDIR)
        mode |= 0755;
    else
        mode |= 0644;
    return mode;
}

uint64_t File::length() const
{
    if (m_type)
        return m_type->read(m_path).size();
    else
        return 0;
}

String File::basename() const
{
    if (m_path.empty())
        return "";
    return *m_path.rbegin();
}

RemoteBuffer File::stat() const
{
    RemoteBuffer stat_data;
    {
        NinePFieldWriter fields{stat_data};
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

RemoteBuffer to_remote_buffer(const char *data)
{
    return RemoteBuffer{ data, data + int(strlen(data)) };
}

RemoteBuffer to_remote_buffer(const StringView& s)
{
    return RemoteBuffer{ s.begin(), s.end() };
}

Context& path_context(const Vector<String>& path)
{
    auto it = std::find_if(ClientManager::instance().begin(),
                           ClientManager::instance().end(),
                           [&](auto& client) { return client->context().name() == path[1]; });
    kak_assert(it != ClientManager::instance().end());
    return (*it)->context();
}

struct ClientCursorByteOffsetFileType : public FileType
{
    RemoteBuffer read(const Vector<String>& path) const
    {
        auto& context = path_context(path);
        auto cursor = context.selections().main().cursor();
        return to_remote_buffer(to_string(context.buffer().distance({0,0}, cursor)));
    }
} client_cursor_byte_offset_file_type{};

struct ClientCursorCharColumnFileType : public FileType
{
    RemoteBuffer read(const Vector<String>& path) const
    {
        auto& context = path_context(path);
        auto coord = context.selections().main().cursor();
        return to_remote_buffer(to_string(context.buffer()[coord.line].char_count_to(coord.column) + 1));
    }
} client_cursor_char_column_file_type{};

struct ClientPidFileType : public FileType
{
    RemoteBuffer read(const Vector<String>& path) const
    {
        auto& context = path_context(path);
        return to_remote_buffer(format("{}", context.client().pid()));
    }
} client_pid_file_type{};

struct NameFileType : public FileType
{
    RemoteBuffer read(const Vector<String>& path) const
    {
        return to_remote_buffer(Server::instance().session());
    }
} name_file_type{};

struct VersionFileType : public FileType
{
    RemoteBuffer read(const Vector<String>& path) const
    {
        extern const char* version;
        return to_remote_buffer(version);
    }
} version_file_type{};

Glob* p(const char* name)
{
    return new Glob(name);
}

struct Entry {
    Vector<Glob*> path;
    FileType* type;
} FILE_ENTRIES[] = {
    { {p("clients")},                                             nullptr },
    { {p("clients"), p("$client_name")},                          nullptr },
    { {p("clients"), p("$client_name"), p("cursor_byte_offset")}, &client_cursor_byte_offset_file_type },
    { {p("clients"), p("$client_name"), p("cursor_char_column")}, &client_cursor_char_column_file_type },
    { {p("clients"), p("$client_name"), p("pid")},                &client_pid_file_type },
    { {p("name")},                                                &name_file_type },
    { {p("version")},                                             &version_file_type },
};

bool is_our_entry(const File* file, const Entry& entry)
{
    if (entry.path.size() != file->path().size() + 1)
        return false;
    auto it_a = file->path().begin();
    auto it_b = entry.path.begin();
    for (; it_a != file->path().end(); ++it_a, ++it_b)
        if (not (*it_b)->matches(*it_a))
            return false;
    return true;
}

Vector<RemoteBuffer> File::contents() const
{
    if (m_type)
    {
        Vector<RemoteBuffer> res;
        res.push_back(m_type->read(m_path));
        return res;
    }
    else
    {
        Vector<RemoteBuffer> res;
        for (const auto& entry : FILE_ENTRIES)
        {
            if (not is_our_entry(this, entry))
                continue;
            Vector<String> parts = (*entry.path.rbegin())->expand();
            for (auto& basename : parts) {
                Vector<String> path{m_path};
                path.push_back(std::move(basename));
                std::unique_ptr<File> file(new File(std::move(path), entry.type));
                res.push_back(file->stat());
            }
        }
        return res;
    }
}

std::unique_ptr<File> File::walk(const String& name) const
{
    for (const auto& entry : FILE_ENTRIES)
    {
        if (not is_our_entry(this, entry))
            continue;
        if (not (*entry.path.rbegin())->matches(name))
            continue;
        Vector<String> path{m_path};
        path.push_back(name);
        return std::unique_ptr<File>(new File(std::move(path), entry.type));
    }
    return {};
}

}
