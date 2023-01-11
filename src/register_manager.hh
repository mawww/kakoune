#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "array_view.hh"
#include "completion.hh"
#include "exception.hh"
#include "utils.hh"
#include "hash_map.hh"
#include "string.hh"
#include "vector.hh"

namespace Kakoune
{

class Context;

class Register
{
public:
    virtual ~Register() = default;

    virtual void set(Context& context, ConstArrayView<String> values, bool restoring = false) = 0;
    virtual ConstArrayView<String> get(const Context& context) = 0;
    virtual const String& get_main(const Context& context, size_t main_index) = 0;

    using RestoreInfo = Vector<String, MemoryDomain::Registers>;
    RestoreInfo save(const Context& context) { return get(context) | gather<RestoreInfo>(); }
    void restore(Context& context, const RestoreInfo& info) { set(context, info, true); }

    NestedBool& modified_hook_disabled() { return m_disable_modified_hook; }

protected:
    NestedBool m_disable_modified_hook;
};

// static value register, which can be modified
class StaticRegister : public Register
{
public:
    StaticRegister(String name) : m_name{std::move(name)} {}

    void set(Context& context, ConstArrayView<String> values, bool) override;
    ConstArrayView<String> get(const Context&) override;
    const String& get_main(const Context& context, size_t main_index) override;

protected:
    String m_name;
    Vector<String, MemoryDomain::Registers> m_content;
};

// Dynamic value register, use it's RegisterRetriever
// to get it's value when needed.
template<typename Getter, typename Setter>
class DynamicRegister : public StaticRegister
{
public:
    DynamicRegister(String name, Getter getter, Setter setter)
        : StaticRegister(std::move(name)), m_getter(std::move(getter)), m_setter(std::move(setter)) {}

    void set(Context& context, ConstArrayView<String> values, bool) override
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

// Register that is used to store some kind prompt history
class HistoryRegister : public StaticRegister
{
public:
    using StaticRegister::StaticRegister;

    void set(Context& context, ConstArrayView<String> values, bool restoring) override;
    const String& get_main(const Context&, size_t) override;
};

template<typename Func>
std::unique_ptr<Register> make_dyn_reg(String name, Func func)
{
    auto setter = [](Context&, ConstArrayView<String>)
    {
        throw runtime_error("this register is not assignable");
    };
    return std::make_unique<DynamicRegister<Func, decltype(setter)>>(name, std::move(func), setter);
}

template<typename Getter, typename Setter>
std::unique_ptr<Register> make_dyn_reg(String name, Getter getter, Setter setter)
{
    return std::make_unique<DynamicRegister<Getter, Setter>>(name, std::move(getter), std::move(setter));
}

class NullRegister : public Register
{
public:
    void set(Context&, ConstArrayView<String>, bool) override {}

    ConstArrayView<String> get(const Context&) override
    {
        return ConstArrayView<String>(String::ms_empty);
    }

    const String& get_main(const Context&, size_t) override
    {
        return String::ms_empty;
    }
};

class RegisterManager : public Singleton<RegisterManager>
{
public:
    Register& operator[](StringView reg) const;
    Register& operator[](Codepoint c) const;
    void add_register(Codepoint c, std::unique_ptr<Register> reg);
    CandidateList complete_register_name(StringView prefix, ByteCount cursor_pos) const;

    auto begin() const { return m_registers.begin(); }
    auto end() const { return m_registers.end(); }

protected:
    HashMap<Codepoint, std::unique_ptr<Register>, MemoryDomain::Registers> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
