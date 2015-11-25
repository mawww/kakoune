#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "array_view.hh"
#include "exception.hh"
#include "utils.hh"
#include "unordered_map.hh"
#include "string.hh"
#include "vector.hh"

#include <functional>

namespace Kakoune
{

class Context;

class Register
{
public:
    virtual ~Register() {}
    virtual Register& operator=(ConstArrayView<String> values) = 0;

    virtual ConstArrayView<String> values(const Context& context) = 0;
};

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
            return ConstArrayView<String>(String::ms_empty);
        else
            return ConstArrayView<String>(m_content);
    }
protected:
    Vector<String, MemoryDomain::Registers> m_content;
};

using RegisterRetriever = std::function<Vector<String, MemoryDomain::Registers> (const Context&)>;

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

class NullRegister : public Register
{
public:
    Register& operator=(ConstArrayView<String> values) override
    {
        return *this;
    }

    ConstArrayView<String> values(const Context& context) override
    {
        return ConstArrayView<String>(String::ms_empty);
    }
};

class RegisterManager : public Singleton<RegisterManager>
{
public:
    Register& operator[](StringView reg) const;
    Register& operator[](Codepoint c) const;
    void add_register(char c, std::unique_ptr<Register> reg);

protected:
    UnorderedMap<char, std::unique_ptr<Register>, MemoryDomain::Registers> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
