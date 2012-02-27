#ifndef register_hh_INCLUDED
#define register_hh_INCLUDED

#include <string>
#include <vector>

#include "memoryview.hh"

namespace Kakoune
{

class Register
{
public:
    Register& operator=(const std::string& value);
    Register& operator=(const memoryview<std::string>& values);

    const std::string& get() const;
    const std::string& get(size_t index) const;

    operator memoryview<std::string>() const
    { return memoryview<std::string>(m_content); }

    const std::vector<std::string>& content() const { return m_content; }
private:
    std::vector<std::string> m_content;

    static const std::string ms_empty;
};

}

#endif // register_hh_INCLUDED

