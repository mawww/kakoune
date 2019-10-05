#include "path_manager.hh"

#include "buffer_manager.hh"
#include "client_manager.hh"
#include "field_writer.hh"
#include "register_manager.hh"
#include "remote.hh"

namespace Kakoune
{

class ContextFinder
{
public:
    using UniqueContextPtr = std::unique_ptr<Context, std::function<void(Context *)>>;

    virtual UniqueContextPtr make_context() const = 0;
};

class GlobalContextFinder : public ContextFinder
{
public:
    GlobalContextFinder()
    {}
    GlobalContextFinder(Vector<String> const& path)
    {}

    UniqueContextPtr make_context() const override
    {
        return UniqueContextPtr(new Context{Context::EmptyContextFlag{}},
                                [](Context *context) { delete context; });
    }
};

class BufferContextFinder : public ContextFinder
{
public:
    BufferContextFinder(StringView buffer_id)
        : m_buffer_id{buffer_id}
    {}
    BufferContextFinder(Vector<String> const& path)
        : m_buffer_id{path[1]}
    {}

    UniqueContextPtr make_context() const override
    {
        Buffer *p = nullptr;
        if (0 == sscanf(m_buffer_id.c_str(), "%p", (void**)&p))
            throw runtime_error("Not found.");

        auto& buffer_manager = BufferManager::instance();
        auto it = std::find(buffer_manager.begin(), buffer_manager.end(), p);
        if (it == buffer_manager.end())
            throw runtime_error("Not found.");

        Selection selection(BufferCoord{0, 0});
        SelectionList selection_list(**it, selection);
        InputHandler input_handler{selection_list, Context::Flags::Draft};
        return UniqueContextPtr(new Context{input_handler, selection_list, Context::Flags::Draft},
                                [](Context *context) { delete context; });
    }

private:
    String m_buffer_id;
};

class WindowContextFinder : public ContextFinder
{
public:
    WindowContextFinder(StringView client_name)
        : m_client_name{client_name}
    {}
    WindowContextFinder(Vector<String> const& path)
        : m_client_name{path[1]}
    {}

    UniqueContextPtr make_context() const override
    {
        auto it = std::find_if(ClientManager::instance().begin(),
                               ClientManager::instance().end(),
                               [&](auto& client) { return client->context().name() == m_client_name; });
        if (it == ClientManager::instance().end())
            throw runtime_error("Not found.");
        auto& context = (*it)->context();
        return UniqueContextPtr(&context, [](Context *) {});
    }

private:
    String m_client_name;
};


class GlobType
{
public:
    static GlobType* resolve(StringView name);

    virtual Vector<String> expand(StringView name) const = 0;

    virtual bool matches(StringView name, StringView text) const
    {
        auto names = expand(name);
        return find(names, text) != names.end();
    }

    virtual std::unique_ptr<ContextFinder> override_context_finder(StringView name) const
    {
        return std::unique_ptr<ContextFinder>{};
    }
};

template<typename BaseGlobType>
struct GlobTypeWithPrefix : public GlobType
{
    GlobTypeWithPrefix(const char* prefix)
        : m_prefix{prefix}
    {}

    bool matches(StringView name, StringView text) const override
    {
        if (text.length() < m_prefix.length())
            return false;
        if (text.substr(0_byte, m_prefix.length()) != m_prefix)
            return false;
        return m_base_glob_type.matches(name, text.substr(m_prefix.length()));
    }

    Vector<String> expand(StringView name) const override
    {
        return m_base_glob_type.expand(name)
            | transform([this](const String& name) -> String { return format("{}{}", m_prefix, name); })
            | gather<Vector<String>>();
    }

    std::unique_ptr<ContextFinder> override_context_finder(StringView name) const override
    {
        return m_base_glob_type.override_context_finder(name);
    }

private:
    BaseGlobType m_base_glob_type{};
    StringView m_prefix;
};

struct LiteralGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const override
    {
        return name == text;
    }

    Vector<String> expand(StringView name) const override
    {
        Vector<String> res;
        res.push_back(String{name});
        return res;
    }
} literal_glob_type{};

struct BufferIdGlobType : public GlobType
{
    Vector<String> expand(StringView name) const override
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

    std::unique_ptr<ContextFinder> override_context_finder(StringView name) const override
    {
        return std::make_unique<BufferContextFinder>(name);
    }
} buffer_id_glob_type{};

struct ClientNameGlobType : public GlobType
{
    Vector<String> expand(StringView name) const override
    {
        Vector<String> res;
        for (auto& client : ClientManager::instance())
            res.push_back(client->context().name());
        return res;
    }

    std::unique_ptr<ContextFinder> override_context_finder(StringView name) const override
    {
        return std::make_unique<WindowContextFinder>(name);
    }
} client_name_glob_type{};

struct OptionNameGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const override
    {
        return GlobalScope::instance().option_registry().option_exists(text);
    }

    Vector<String> expand(StringView name) const override
    {
        return GlobalScope::instance().option_registry().complete_option_name("", 0_byte)
            | gather<Vector<String>>();
    }
};

struct RegisterNameGlobType : public GlobType
{
    Vector<String> expand(StringView name) const override
    {
        Vector<String> res;
        for (auto& register_entry : RegisterManager::instance())
            res.push_back(RegisterManager::name(register_entry.key));
        return res;
    }
};

GlobTypeWithPrefix<OptionNameGlobType> opt_name_glob_type{"opt_"};
GlobTypeWithPrefix<RegisterNameGlobType> reg_name_glob_type{"reg_"};
GlobTypeWithPrefix<RegisterNameGlobType> main_reg_name_glob_type{"main_reg_"};

GlobType* GlobType::resolve(StringView name)
{
    if (name == "$buffer_id")   return &buffer_id_glob_type;
    if (name == "$client_name") return &client_name_glob_type;
    if (name == "$main_reg")    return &main_reg_name_glob_type;
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

    std::shared_ptr<ContextFinder> override_context_finder(StringView name, std::shared_ptr<ContextFinder> old_context_finder)
    {
        std::shared_ptr<ContextFinder> new_context_finder = GlobType::resolve(name)->override_context_finder(name);
        if (new_context_finder)
            return new_context_finder;
        return old_context_finder;
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
    : m_path{}, m_component{&root}, m_context_finder{std::make_unique<GlobalContextFinder>()}
{
}

File::File(Vector<String> path, Glob* component, std::shared_ptr<ContextFinder> context_finder)
    : m_path{path}, m_component{component}, m_context_finder{context_finder}
{
}

File::~File()
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
        std::shared_ptr<ContextFinder> context_finder = child->override_context_finder(name, m_context_finder);
        return std::unique_ptr<File>(new File(std::move(path), child, context_finder));
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
                auto context_finder = child->override_context_finder(basename, m_context_finder); 
                path.push_back(std::move(basename));
                std::unique_ptr<File> file(new File(std::move(path), child, context_finder));
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

template<typename ContextPolicy>
class VarFileType : public FileType
{
public:
    VarFileType(ConstArrayView<EnvVarDesc> env_vars)
        : m_env_vars{env_vars}
    {
    }

    RemoteBuffer read(const Vector<String>& path) const
    {
        String varname = path.back();
        ContextFinder::UniqueContextPtr context_ptr = ContextPolicy(path).make_context();
        return to_remote_buffer(find_env_var(varname).func(varname, *context_ptr, Quoting::Shell));
    }

private:
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

    ConstArrayView<EnvVarDesc> m_env_vars;
};

void register_paths(ConstArrayView<EnvVarDesc> builtin_env_vars)
{
    auto* global_var_file_type = new VarFileType<GlobalContextFinder>{builtin_env_vars};
    auto* buffer_var_file_type = new VarFileType<BufferContextFinder>{builtin_env_vars};
    auto* window_var_file_type = new VarFileType<WindowContextFinder>{builtin_env_vars};
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
    root.register_path({"windows", "$client_name", "$main_reg"}, window_var_file_type);
    root.register_path({"windows", "$client_name", "$reg"}, window_var_file_type);
}

}
