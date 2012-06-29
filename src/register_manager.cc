#include "register_manager.hh"

#include "utils.hh"
#include "assert.hh"

namespace Kakoune
{

// static value register, which can be modified
// using operator=, so should be user modifiable
class StaticRegister : public Register
{
public:
    Register& operator=(const memoryview<String>& values)
    {
        m_content = std::vector<String>(values.begin(), values.end());
        return *this;
    }

    const String& operator[](size_t index)
    {
        if (m_content.size() > index)
            return m_content[index];
        else
            return ms_empty;
    }

    operator memoryview<String>()
    {
        return memoryview<String>(m_content);
    }
protected:
    std::vector<String> m_content;

    static const String ms_empty;
};

const String StaticRegister::ms_empty;

// Dynamic value register, use it's RegisterRetriever
// to get it's value when needed.
class DynamicRegister : public StaticRegister
{
public:
    DynamicRegister(RegisterRetriever function)
        : m_function(std::move(function)) {}

    Register& operator=(const memoryview<String>& values)
    {
        throw runtime_error("this register is not assignable");
    }

    const String& operator[](size_t index)
    {
        m_content = m_function();
        return StaticRegister::operator[](index);
    }

    operator memoryview<String>()
    {
        m_content = m_function();
        return StaticRegister::operator memoryview<String>();
    }

private:
    RegisterRetriever   m_function;
};

Register& RegisterManager::operator[](char reg)
{
    auto& reg_ptr = m_registers[reg];
    if (not reg_ptr)
        reg_ptr.reset(new StaticRegister());
    return *reg_ptr;
}

void RegisterManager::register_dynamic_register(char reg, RegisterRetriever function)
{
    auto& reg_ptr = m_registers[reg];
    assert(not reg_ptr);
    reg_ptr.reset(new DynamicRegister(std::move(function)));
}

}
