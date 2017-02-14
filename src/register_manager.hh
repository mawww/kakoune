#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "array_view.hh"
#include "exception.hh"
#include "utils.hh"
#include "unordered_map.hh"
#include "string.hh"
#include "vector.hh"

namespace Kakoune
{

class Context;

class Register
{
public:
    virtual ~Register() = default;

    virtual void set(Context& context, ConstArrayView<String> values) = 0;
    virtual ConstArrayView<String> get(const Context& context) = 0;
};

// static value register, which can be modified
// using operator=, so should be user modifiable
class StaticRegister : public Register
{
public:
    void set(Context&, ConstArrayView<String> values) override
    {
        m_content = Vector<String, MemoryDomain::Registers>(values.begin(), values.end());
    }

    ConstArrayView<String> get(const Context&) override
    {
        if (m_content.empty())
            return ConstArrayView<String>(String::ms_empty);
        else
            return ConstArrayView<String>(m_content);
    }
protected:
    Vector<String, MemoryDomain::Registers> m_content;
};

// Dynamic value register, use it's RegisterRetriever
// to get it's value when needed.
template<typename Getter, typename Setter>
class DynamicRegister : public StaticRegister
{
public:
    DynamicRegister(Getter getter, Setter setter)
        : m_getter{std::move(getter)}, m_setter{std::move(setter)} {}

    void set(Context& context, ConstArrayView<String> values) override
    {
        m_setter(context, values);
    }

    ConstArrayView<String> get(const Context& context) override
    {
        m_content = m_getter(context);
        return StaticRegister::get(context);
    }

private:
    Getter m_getter;
    Setter m_setter;
};

template<typename Func>
std::unique_ptr<Register> make_dyn_reg(Func func)
{
    auto setter = [](Context&, ConstArrayView<String>)
    {
        throw runtime_error("this register is not assignable");
    };
    return make_unique<DynamicRegister<Func, decltype(setter)>>(std::move(func), setter);
}

template<typename Getter, typename Setter>
std::unique_ptr<Register> make_dyn_reg(Getter getter, Setter setter)
{
    return make_unique<DynamicRegister<Getter, Setter>>(std::move(getter), std::move(setter));
}

class NullRegister : public Register
{
public:
    void set(Context&, ConstArrayView<String>) override {}

    ConstArrayView<String> get(const Context&) override
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
