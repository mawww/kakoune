#include "register.hh"

namespace Kakoune
{

const std::string Register::ms_empty;

Register& Register::operator=(const std::string& value)
{
    m_content.clear();
    m_content.push_back(value);
    return *this;
}

Register& Register::operator=(const memoryview<std::string>& values)
{
    m_content = std::vector<std::string>(values.begin(), values.end());
    return *this;
}

const std::string& Register::get() const
{
    if (m_content.size() != 0)
        return m_content.front();
    else
        return ms_empty;
}

const std::string& Register::get(size_t index) const
{
    if (m_content.size() > index)
        return m_content[index];
    else
        return ms_empty;
}

}
