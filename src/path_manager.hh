#ifndef path_manager_hh_INCLUDED
#define path_manager_hh_INCLUDED

#include "field_writer.hh"
#include "string.hh"
#include "string_utils.hh" // FIXME
#include "utils.hh"
#include "vector.hh"

namespace Kakoune
{

class Glob
{
public:
    Glob(StringView name);

    bool matches(StringView text) const;
    Vector<String> expand() const;

private:
    String m_name;
};

class FileType
{
public:
    virtual RemoteBuffer read(const Vector<String>& path) const = 0;
};

class File {
private:
    static Glob* p(const char* name)
    {
        return new Glob(name);
    }

    struct Entry {
        Vector<Glob*> path;
        FileType* type;
    };

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

    File(Vector<String> path, FileType* type);

    std::unique_ptr<File> walk(const String& name) const;

    Vector<RemoteBuffer> contents() const;
    Type type() const;
    Vector<String> path() const;
    String fullname() const;
    Qid qid() const;
    uint32_t mode() const;
    uint64_t length() const;
    String basename() const;
    RemoteBuffer stat() const;

private:
    Vector<String> m_path;
    FileType* m_type;
    static Entry m_entries[];

    bool is_our_entry(const Entry& entry) const;
};

}

#endif // path_manager_hh_INCLUDED

