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
    Register& operator=(const memoryview<String>& values) override
    {
        m_content = std::vector<String>(values.begin(), values.end());
        return *this;
    }

    memoryview<String> values(const Context&) override
    {
        if (m_content.empty())
            return memoryview<String>(ms_empty);
        else
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

    Register& operator=(const memoryview<String>& values) override
    {
        throw runtime_error("this register is not assignable");
    }

    memoryview<String> values(const Context& context) override
    {
        m_content = m_function(context);
        return StaticRegister::values(context);
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
    kak_assert(not reg_ptr);
    reg_ptr.reset(new DynamicRegister(std::move(function)));
}

}
