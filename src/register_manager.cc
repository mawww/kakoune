#include "register_manager.hh"

#include "assert.hh"
#include "exception.hh"
#include "id_map.hh"

namespace Kakoune
{

// static value register, which can be modified
// using operator=, so should be user modifiable
class StaticRegister : public Register
{
public:
    Register& operator=(ConstArrayView<String> values) override
    {
        m_content = Vector<String, MemoryDomain::Registers>(values.begin(), values.end());
        return *this;
    }

    ConstArrayView<String> values(const Context&) override
    {
        if (m_content.empty())
            return ConstArrayView<String>(ms_empty);
        else
            return ConstArrayView<String>(m_content);
    }
protected:
    Vector<String, MemoryDomain::Registers> m_content;

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

    Register& operator=(ConstArrayView<String> values) override
    {
        throw runtime_error("this register is not assignable");
    }

    ConstArrayView<String> values(const Context& context) override
    {
        m_content = m_function(context);
        return StaticRegister::values(context);
    }

private:
    RegisterRetriever m_function;
};

Register& RegisterManager::operator[](StringView reg)
{
    if (reg.length() == 1)
        return (*this)[reg[0_byte]];

    static const IdMap<Codepoint> reg_names = {
        { "slash", '/' },
        { "dquote", '"' },
        { "pipe", '|' },
        { "caret", '^' },
        { "arobase", '@' }
    };
    auto it = reg_names.find(reg);
    if (it == reg_names.end())
        throw runtime_error(format("no such register: '{}'", reg));
    return (*this)[it->second];
}

Register& RegisterManager::operator[](Codepoint c)
{
    c = tolower(c);
    if (c < 32 or c > 127)
        throw runtime_error(format("invalid register name: '{}'", c));

    auto& reg_ptr = m_registers[c];
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
