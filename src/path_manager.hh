#ifndef path_manager_hh_INCLUDED
#define path_manager_hh_INCLUDED

#include "string.hh"
#include "utils.hh"
#include "vector.hh"

namespace Kakoune
{

class GlobType
{
public:
    static GlobType* resolve(StringView name);

    virtual bool matches(StringView name, StringView text) const = 0;
    virtual Vector<String> expand(StringView name) const = 0;
};

}

#endif // path_manager_hh_INCLUDED

