#include "register.hh"

namespace Kakoune
{

const String Register::ms_empty;

Register& Register::operator=(const String& value)
{
    m_content.clear();
    m_content.push_back(value);
    return *this;
}

Register& Register::operator=(const memoryview<String>& values)
{
    m_content = std::vector<String>(values.begin(), values.end());
    return *this;
}

const String& Register::get() const
{
    if (m_content.size() != 0)
        return m_content.front();
    else
        return ms_empty;
}

const String& Register::get(size_t index) const
{
    if (m_content.size() > index)
        return m_content[index];
    else
        return ms_empty;
}

}
