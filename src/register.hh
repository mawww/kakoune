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
    void set(const std::string& value);
    void set(const memoryview<std::string>& values);
    const std::string& get() const;
    const std::string& get(size_t index) const;

private:
    std::vector<std::string> m_content;

    static const std::string ms_empty;
};

}

#endif // register_hh_INCLUDED

