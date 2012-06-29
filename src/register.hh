#ifndef register_hh_INCLUDED
#define register_hh_INCLUDED

#include <vector>

#include "string.hh"
#include "memoryview.hh"

namespace Kakoune
{

class Register
{
public:
    Register& operator=(const memoryview<String>& values);

    const String& operator[](size_t index) const;

    operator memoryview<String>() const
    { return memoryview<String>(m_content); }
private:
    std::vector<String> m_content;

    static const String ms_empty;
};

}

#endif // register_hh_INCLUDED

