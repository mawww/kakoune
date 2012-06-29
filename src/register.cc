#include "register.hh"

namespace Kakoune
{

const String Register::ms_empty;

Register& Register::operator=(const memoryview<String>& values)
{
    m_content = std::vector<String>(values.begin(), values.end());
    return *this;
}

const String& Register::operator[](size_t index) const
{
    if (m_content.size() > index)
        return m_content[index];
    else
        return ms_empty;
}

}
