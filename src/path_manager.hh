#ifndef path_manager_hh_INCLUDED
#define path_manager_hh_INCLUDED

#include "field_writer.hh"
#include "shell_manager.hh"
#include "string.hh"
#include "utils.hh"
#include "vector.hh"

namespace Kakoune
{

using ContextualAction = std::function<void(Context&)>;
using ContextGetter = std::function<void(const ContextualAction&)>;

class Glob;

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

    File();
    ~File();

    std::unique_ptr<File> walk(const String& name) const;

    Vector<RemoteBuffer> contents() const;
    Type type() const;
    const Vector<String>& path() const;
    String fullname() const;
    Qid qid() const;
    uint32_t mode() const;
    uint64_t length() const;
    String basename() const;
    RemoteBuffer stat() const;

private:
    File(Vector<String> path, Glob* component, ContextGetter context_getter);

private:
    Vector<String> m_path;
    Glob* m_component;
    ContextGetter m_context_getter;
};

void register_paths(ConstArrayView<EnvVarDesc> builtin_env_vars);

}

#endif // path_manager_hh_INCLUDED

