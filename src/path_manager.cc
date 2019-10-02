#include "path_manager.hh"

#include "buffer_manager.hh"
#include "client_manager.hh"
#include "field_writer.hh"
#include "register_manager.hh"
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

struct BufferIdGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        String s{text};
        Buffer* p;
        if (sscanf(s.c_str(), "%p", (void**)&p) <= 0)
            return false;
        auto it = find(BufferManager::instance(), p);
        return it != BufferManager::instance().end();
    }

    Vector<String> expand(StringView name) const
    {
        Vector<String> res;
        for (auto& buffer : BufferManager::instance())
        {
            char id[25];
            snprintf(id, sizeof(id), "%p", (void*)&*buffer);
            res.push_back(String{id});
        }
        return res;
    }
} buffer_id_glob_type{};

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

struct OptNameGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        if (text.length() < 4)
            return false;
        if (text.substr(0_byte, 4_byte) != "opt_")
            return false;
        return GlobalScope::instance().option_registry().option_exists(text.substr(4_byte));
    }

    Vector<String> expand(StringView name) const
    {
        return GlobalScope::instance().option_registry().complete_option_name("", 0_byte)
            | transform([](const String& name) -> String { return format("opt_{}", name); })
            | gather<Vector<String>>();
    }
} opt_name_glob_type{};

struct RegNameGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        if (text.length() < 4)
            return false;
        if (text.substr(0_byte, 4_byte) != "reg_")
            return false;
        auto reg_name = text.substr(4_byte);
        auto it = find_if(RegisterManager::instance(),
                          [&reg_name](auto& item) { return RegisterManager::name(item.key) == reg_name; });
        return it != RegisterManager::instance().end();
    }

    Vector<String> expand(StringView name) const
    {
        Vector<String> res;
        for (auto& register_entry : RegisterManager::instance())
            res.push_back(format("reg_{}", RegisterManager::name(register_entry.key)));
        return res;
    }
} reg_name_glob_type{};

GlobType* GlobType::resolve(StringView name)
{
    if (name == "$buffer_id")   return &buffer_id_glob_type;
    if (name == "$client_name") return &client_name_glob_type;
    if (name == "$opt")         return &opt_name_glob_type;
    if (name == "$reg")         return &reg_name_glob_type;
    return &literal_glob_type;
}

class FileType
{
public:
    virtual RemoteBuffer read(const Vector<String>& path) const = 0;
};

class Glob
{
public:
    Glob(StringView name)
        : m_name{name}, m_type{nullptr}
    {}

    FileType* type() const
    {
        return m_type;
    }

    const Vector<Glob*>& children() const
    {
        return m_children;
    }

    bool matches(StringView text) const
    {
        return GlobType::resolve(m_name)->matches(m_name, text);
    }

    Vector<String> expand() const
    {
        return GlobType::resolve(m_name)->expand(m_name);
    }

    void register_path(Vector<StringView> path, FileType* type)
    {
        Glob* node = this;
        for (auto& path_segment : path)
        {
            // Can only add children to directories
            kak_assert(node->m_type == nullptr);

            auto it = find_if(node->m_children, [&](auto& node) { return node->m_name == path_segment; });
            if (it != node->m_children.end())
                node = *it;
            else
            {
                Glob *next = new Glob{path_segment};
                node->m_children.push_back(next);
                node = next;
            }
        }

        // Can't register the same path twice
        kak_assert(node->m_type == nullptr);
        // Can't make an internal node a non-directory
        kak_assert(node->m_children.empty());

        node->m_type = type;
    }

private:
    String m_name;
    FileType* m_type;
    Vector<Glob*> m_children;
};

Glob root{"/"};

// File

File::File()
    : m_path{}, m_component{&root}
{
}

File::File(Vector<String> path, Glob* component)
    : m_path{path}, m_component{component}
{
}

std::unique_ptr<File> File::walk(const String& name) const
{
    for (const auto& child : m_component->children())
    {
        if (not child->matches(name))
            continue;
        Vector<String> path{m_path};
        path.push_back(name);
        return std::unique_ptr<File>(new File(std::move(path), child));
    }
    return {};
}

Vector<RemoteBuffer> File::contents() const
{
    if (m_component->type())
    {
        Vector<RemoteBuffer> res;
        res.push_back(m_component->type()->read(m_path));
        return res;
    }
    else
    {
        Vector<RemoteBuffer> res;
        for (const auto& child : m_component->children())
        {
            Vector<String> parts = child->expand();
            for (auto& basename : parts) {
                Vector<String> path{m_path};
                path.push_back(std::move(basename));
                std::unique_ptr<File> file(new File(std::move(path), child));
                res.push_back(file->stat());
            }
        }
        return res;
    }
}

File::Type File::type() const
{
    if (not m_component->type())
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
    if (m_component->type())
        return m_component->type()->read(m_path).size();
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
        result_fields.write(Raw{stat_data});
    }
    return result;
}

RemoteBuffer to_remote_buffer(const StringView& s)
{
    return RemoteBuffer{ s.begin(), s.end() };
}

struct GlobalContext
{
    static RemoteBuffer with_context(const Vector<String>& path, std::function<RemoteBuffer (Context&)> f)
    {
        Context context{Context::EmptyContextFlag{}};
        return f(context);
    }
};

struct BufferContext
{
    static RemoteBuffer with_context(const Vector<String>& path, std::function<RemoteBuffer (Context&)> f)
    {
        Buffer *p = nullptr;
        if (0 == sscanf(path[1].c_str(), "%p", (void**)&p))
            throw runtime_error("Not found.");

        auto& buffer_manager = BufferManager::instance();
        auto it = std::find(buffer_manager.begin(), buffer_manager.end(), p);
        if (it == buffer_manager.end())
            throw runtime_error("Not found.");

        Selection selection(BufferCoord{0, 0});
        SelectionList selection_list(**it, selection);
        InputHandler input_handler{selection_list, Context::Flags::Draft};
        Context context{input_handler, selection_list, Context::Flags::Draft};
        return f(context);
    }
};

struct WindowContext
{
    static RemoteBuffer with_context(const Vector<String>& path, std::function<RemoteBuffer (Context&)> f)
    {
        auto it = std::find_if(ClientManager::instance().begin(),
                               ClientManager::instance().end(),
                               [&](auto& client) { return client->context().name() == path[1]; });
        if (it == ClientManager::instance().end())
            throw runtime_error("Not found.");
        auto& context = (*it)->context();
        return f(context);
    }
};

template<typename ContextPolicy>
class VarFileType : public FileType
{
public:
    VarFileType(ConstArrayView<EnvVarDesc> env_vars)
        : m_env_vars{env_vars}
    {
    }

    const EnvVarDesc& find_env_var(StringView varname) const
    {
        for (auto& env_var : m_env_vars)
        {
            if ((not env_var.prefix and env_var.str == varname) or
                (env_var.prefix and varname.length() > env_var.str.length() and
                 varname.substr(0_byte, env_var.str.length()) == env_var.str))
                return env_var;
        }
        throw logic_error();
    }

    RemoteBuffer read(const Vector<String>& path) const
    {
        String varname = path.back();
        return ContextPolicy::with_context(path, [&](Context& context) -> RemoteBuffer {
            return to_remote_buffer(find_env_var(varname).func(varname, context, Quoting::Shell));
        });
    }

private:
    ConstArrayView<EnvVarDesc> m_env_vars;
};

void register_paths(ConstArrayView<EnvVarDesc> builtin_env_vars)
{
    auto* global_var_file_type = new VarFileType<GlobalContext>{builtin_env_vars};
    auto* buffer_var_file_type = new VarFileType<BufferContext>{builtin_env_vars};
    auto* window_var_file_type = new VarFileType<WindowContext>{builtin_env_vars};
    for (auto& env_var : builtin_env_vars)
    {
        if (env_var.prefix)
            continue;
        if (env_var.scopes & EnvVarDesc::Scopes::Global)
            root.register_path({"global", env_var.str}, global_var_file_type);
        if (env_var.scopes & EnvVarDesc::Scopes::Buffer)
            root.register_path({"buffers", "$buffer_id", env_var.str}, buffer_var_file_type);
        if (env_var.scopes & EnvVarDesc::Scopes::Buffer or env_var.scopes & EnvVarDesc::Scopes::Window)
            root.register_path({"windows", "$client_name", env_var.str}, window_var_file_type);
    }
    root.register_path({"global", "$opt"}, global_var_file_type);
    root.register_path({"buffers", "$buffer_id", "$opt"}, buffer_var_file_type);
    root.register_path({"windows", "$client_name", "$opt"}, window_var_file_type);
    root.register_path({"windows", "$client_name", "$reg"}, window_var_file_type);
}

}
