#ifndef option_manager_hh_INCLUDED
#define option_manager_hh_INCLUDED

#include "completion.hh"
#include "containers.hh"
#include "exception.hh"
#include "flags.hh"
#include "option_types.hh"
#include "vector.hh"

#include <memory>
#include <type_traits>

namespace Kakoune
{

class OptionManager;

enum class OptionFlags
{
    None   = 0,
    Hidden = 1,
};

template<> struct WithBitOps<OptionFlags> : std::true_type {};

class OptionDesc
{
public:
    OptionDesc(String name, String docstring, OptionFlags flags);

    const String& name() const { return m_name; }
    const String& docstring() const { return m_docstring; }

    OptionFlags flags() const { return m_flags; }

private:
    String m_name;
    String m_docstring;
    OptionFlags  m_flags;
};

class Option
{
public:
    virtual ~Option() = default;

    template<typename T> const T& get() const;
    template<typename T> T& get_mutable();
    template<typename T> void set(const T& val, bool notify=true);
    template<typename T> bool is_of_type() const;

    virtual String get_as_string() const = 0;
    virtual void   set_from_string(StringView str) = 0;
    virtual void   add_from_string(StringView str) = 0;

    virtual Option* clone(OptionManager& manager) const = 0;
    OptionManager& manager() const { return m_manager; }

    const String& name() const { return m_desc.name(); }
    const String& docstring() const { return m_desc.docstring(); }
    OptionFlags flags() const { return m_desc.flags(); }

protected:
    Option(const OptionDesc& desc, OptionManager& manager);

    OptionManager& m_manager;
    const OptionDesc& m_desc;
};

class OptionManagerWatcher
{
public:
    virtual ~OptionManagerWatcher() {}

    virtual void on_option_changed(const Option& option) = 0;
};

class OptionManager : private OptionManagerWatcher
{
public:
    OptionManager(OptionManager& parent);
    ~OptionManager();

    Option& operator[] (StringView name);
    const Option& operator[] (StringView name) const;
    Option& get_local_option(StringView name);

    void unset_option(StringView name);

    using OptionList = Vector<const Option*>;
    OptionList flatten_options() const;

    void register_watcher(OptionManagerWatcher& watcher) const;
    void unregister_watcher(OptionManagerWatcher& watcher) const;

    void on_option_changed(const Option& option) override;
private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class Scope;
    friend class OptionsRegistry;

    Vector<std::unique_ptr<Option>, MemoryDomain::Options> m_options;
    OptionManager* m_parent;

    mutable Vector<OptionManagerWatcher*, MemoryDomain::Options> m_watchers;
};

template<typename T>
class TypedOption : public Option
{
public:
    TypedOption(OptionManager& manager, const OptionDesc& desc, const T& value)
        : Option(desc, manager), m_value(value) {}

    void set(T value, bool notify = true)
    {
        validate(value);
        if (m_value != value)
        {
            m_value = std::move(value);
            if (notify)
                manager().on_option_changed(*this);
        }
    }
    const T& get() const { return m_value; }
    T& get_mutable() { return m_value; }

    String get_as_string() const override
    {
        return option_to_string(m_value);
    }
    void set_from_string(StringView str) override
    {
        T val;
        option_from_string(str, val);
        set(std::move(val));
    }
    void add_from_string(StringView str) override
    {
        if (option_add(m_value, str))
            m_manager.on_option_changed(*this);
    }

    using Alloc = Allocator<TypedOption, MemoryDomain::Options>;
    static void* operator new (std::size_t sz)
    {
        kak_assert(sz == sizeof(TypedOption));
        return Alloc{}.allocate(1);
    }

    static void operator delete (void* ptr)
    {
        return Alloc{}.deallocate(reinterpret_cast<TypedOption*>(ptr), 1);
    }
private:
    virtual void validate(const T& value) const {}
    T m_value;
};

template<typename T, void (*validator)(const T&)>
class TypedCheckedOption : public TypedOption<T>
{
    using TypedOption<T>::TypedOption;

    Option* clone(OptionManager& manager) const override
    {
        return new TypedCheckedOption{manager, this->m_desc, this->get()};
    }

    void validate(const T& value) const override { if (validator != nullptr) validator(value); }
};

template<typename T> const T& Option::get() const
{
    auto* typed_opt = dynamic_cast<const TypedOption<T>*>(this);
    if (not typed_opt)
        throw runtime_error(format("option '{}' is not of type '{}'", name(), typeid(T).name()));
    return typed_opt->get();
}

template<typename T> T& Option::get_mutable()
{
    return const_cast<T&>(get<T>());
}

template<typename T> void Option::set(const T& val, bool notify)
{
    auto* typed_opt = dynamic_cast<TypedOption<T>*>(this);
    if (not typed_opt)
        throw runtime_error(format("option '{}' is not of type '{}'", name(), typeid(T).name()));
    return typed_opt->set(val, notify);
}

template<typename T> bool Option::is_of_type() const
{
    return dynamic_cast<const TypedOption<T>*>(this) != nullptr;
}

template<typename T>
auto find_option(T& container, StringView name) -> decltype(container.begin())
{
    using ptr_type = decltype(*container.begin());
    return find_if(container, [&name](const ptr_type& opt) { return opt->name() == name; });
}

class OptionsRegistry
{
public:
    OptionsRegistry(OptionManager& global_manager) : m_global_manager(global_manager) {}

    template<typename T, void (*validator)(const T&) = nullptr>
    Option& declare_option(StringView name, StringView docstring,
                           const T& value,
                           OptionFlags flags = OptionFlags::None)
    {
        auto& opts = m_global_manager.m_options;
        auto it = find_option(opts, name);
        if (it != opts.end())
        {
            if ((*it)->is_of_type<T>() and (*it)->flags() == flags)
                return **it;
            throw runtime_error(format("option '{}' already declared with different type or flags", name));
        }
        String doc =  docstring.empty() ? format("[{}]", option_type_name<T>::name())
                                        : format("[{}] - {}", option_type_name<T>::name(), docstring);
        m_descs.emplace_back(new OptionDesc{name.str(), std::move(doc), flags});
        opts.emplace_back(new TypedCheckedOption<T, validator>{m_global_manager, *m_descs.back(), value});
        return *opts.back();
    }

    const OptionDesc* option_desc(StringView name) const
    {
        auto it = find_if(m_descs,
                          [&name](const std::unique_ptr<const OptionDesc>& opt)
                          { return opt->name() == name; });
        return it != m_descs.end() ? it->get() : nullptr;
    }

    bool option_exists(StringView name) const { return option_desc(name) != nullptr; }

    CandidateList complete_option_name(StringView prefix, ByteCount cursor_pos) const;
private:
    OptionManager& m_global_manager;
    Vector<std::unique_ptr<const OptionDesc>, MemoryDomain::Options> m_descs;
};

}

#endif // option_manager_hh_INCLUDED
