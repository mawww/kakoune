#ifndef path_manager_hh_INCLUDED
#define path_manager_hh_INCLUDED

#include "field_writer.hh"
#include "string.hh"
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

}

#endif // path_manager_hh_INCLUDED

