#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "array_view.hh"
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

    virtual void set(Context& context, ConstArrayView<String> values) = 0;
    virtual ConstArrayView<String> get(const Context& context) = 0;
    virtual const String& get_main(const Context& context, size_t main_index) = 0;

    struct RestoreInfo
    {
        std::vector<String> data;
        size_t size;
    };
    virtual RestoreInfo save(const Context&) = 0;
    virtual void restore(Context&, const RestoreInfo&) = 0;
};

// static value register, which can be modified
// using operator=, so should be user modifiable
class StaticRegister : public Register
{
public:
    void set(Context&, ConstArrayView<String> values) override
    {
        m_content.assign(values.begin(), values.end());
    }

    ConstArrayView<String> get(const Context&) override
    {
        if (m_content.empty())
            return ConstArrayView<String>(String::ms_empty);
        else
            return ConstArrayView<String>(m_content);
    }

    const String& get_main(const Context& context, size_t main_index) override
    {
        return get(context)[std::min(main_index, m_content.size() - 1)];
    }

    RestoreInfo save(const Context& context) override
    {
        //std::unique_ptr<String[]> data{new String[m_content.size()]};
        //std::copy_n(m_content.data(), m_content.size(), data.get());
        auto content = get(context);
        std::vector<String> data{content.begin(), content.end()};
        return {std::move(data), content.size()};
    }

    void restore(Context&, const RestoreInfo& info) override
    {
        m_content.assign(info.data.begin(), info.data.begin() + info.size);
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
        : m_getter(std::move(getter)), m_setter(std::move(setter)) {}

    void set(Context& context, ConstArrayView<String> values) override
    {
        m_setter(context, values);
    }

    ConstArrayView<String> get(const Context& context) override
    {
        m_content = m_getter(context);
        return StaticRegister::get(context);
    }

    void restore(Context& context, const RestoreInfo& info) override
    {
        set(context, info.data);
    }

private:
    Getter m_getter;
    Setter m_setter;
};

// Register that is used to store some kind prompt history
class HistoryRegister : public StaticRegister
{
public:
    void set(Context&, ConstArrayView<String> values) override
    {
        for (auto& entry : values)
        {
            m_content.erase(std::remove(m_content.begin(), m_content.end(), entry),
                          m_content.end());
            m_content.push_back(entry);
        }
    }

    const String& get_main(const Context&, size_t) override
    {
        return m_content.empty() ? String::ms_empty : m_content.back();
    }

    RestoreInfo save(const Context&) override
    {
        return {{}, m_content.size()};
    }

    void restore(Context&, const RestoreInfo& info) override
    {
        if (info.size < m_content.size())
            m_content.resize(info.size);
    }
};

template<typename Func>
std::unique_ptr<Register> make_dyn_reg(Func func)
{
    auto setter = [](Context&, ConstArrayView<String>)
    {
        throw runtime_error("this register is not assignable");
    };
    return std::make_unique<DynamicRegister<Func, decltype(setter)>>(std::move(func), setter);
}

template<typename Getter, typename Setter>
std::unique_ptr<Register> make_dyn_reg(Getter getter, Setter setter)
{
    return std::make_unique<DynamicRegister<Getter, Setter>>(std::move(getter), std::move(setter));
}

class NullRegister : public Register
{
public:
    void set(Context&, ConstArrayView<String>) override {}

    ConstArrayView<String> get(const Context&) override
    {
        return ConstArrayView<String>(String::ms_empty);
    }

    const String& get_main(const Context&, size_t) override
    {
        return String::ms_empty;
    }

    RestoreInfo save(const Context&) override { return {}; }
    void restore(Context&, const RestoreInfo& info) override {}
};

class RegisterManager : public Singleton<RegisterManager>
{
public:
    Register& operator[](StringView reg) const;
    Register& operator[](Codepoint c) const;
    void add_register(Codepoint c, std::unique_ptr<Register> reg);

protected:
    HashMap<Codepoint, std::unique_ptr<Register>, MemoryDomain::Registers> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
