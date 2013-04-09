#ifndef option_manager_hh_INCLUDED
#define option_manager_hh_INCLUDED

#include "completion.hh"
#include "exception.hh"
#include "option_types.hh"
#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

struct option_not_found : public runtime_error
{
    option_not_found(const String& name)
        : runtime_error("option not found: " + name) {}
};

class OptionManager;

class Option
{
public:
    Option(OptionManager& manager, String name);
    virtual ~Option() {}

    template<typename T> const T& get() const;
    template<typename T> void set(const T& val);

    virtual String get_as_string() const = 0;
    virtual void   set_from_string(const String& str) = 0;
    virtual void   add_from_string(const String& str) = 0;

    String name() const { return m_name; }
    OptionManager& manager() const { return m_manager; }

    virtual Option* clone(OptionManager& manager) const = 0;
protected:
    OptionManager& m_manager;
    String m_name;
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

    const Option& operator[] (const String& name) const;
    Option& get_local_option(const String& name);

    CandidateList complete_option_name(const String& prefix,
                                       ByteCount cursor_pos);

    using OptionList = std::vector<const Option*>;
    OptionList flatten_options() const;

    void register_watcher(OptionManagerWatcher& watcher);
    void unregister_watcher(OptionManagerWatcher& watcher);

    void on_option_changed(const Option& option) override;
private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class GlobalOptions;

    std::vector<std::unique_ptr<Option>> m_options;
    OptionManager* m_parent;

    std::vector<OptionManagerWatcher*> m_watchers;
};

template<typename T>
class TypedOption : public Option
{
public:
    TypedOption(OptionManager& manager, String name, const T& value)
        : Option(manager, std::move(name)), m_value(value) {}

    void set(const T& value)
    {
        if (m_value != value)
        {
            m_value = value;
            m_manager.on_option_changed(*this);
        }
    }
    const T& get() const { return m_value; }

    String get_as_string() const override
    {
        return option_to_string(m_value);
    }
    void set_from_string(const String& str) override
    {
        T val;
        option_from_string(str, val);
        set(val);
    }
    void add_from_string(const String& str) override
    {
        T val;
        option_from_string(str, val);
        if (option_add(m_value, val))
            m_manager.on_option_changed(*this);
    }

    Option* clone(OptionManager& manager) const override
    {
        return new TypedOption{manager, name(), m_value};
    }
private:
    T m_value;
};

template<typename T> const T& Option::get() const
{
    auto* typed_opt = dynamic_cast<const TypedOption<T>*>(this);
    if (not typed_opt)
        throw runtime_error("option " + name() + " is not of type " + typeid(T).name());
    return typed_opt->get();
}

template<typename T> void Option::set(const T& val)
{
    auto* typed_opt = dynamic_cast<TypedOption<T>*>(this);
    if (not typed_opt)
        throw runtime_error("option " + name() + " is not of type " + typeid(T).name());
    return typed_opt->set(val);
}

template<typename T>
auto find_option(T& container, const String& name) -> decltype(container.begin())
{
    using ptr_type = decltype(*container.begin());
    return find_if(container, [&name](const ptr_type& opt) { return opt->name() == name; });
}

class GlobalOptions : public OptionManager,
                      public Singleton<GlobalOptions>
{
public:
    GlobalOptions();

    template<typename T>
    Option& declare_option(const String& name, const T& value)
    {
        if (find_option(m_options, name) != m_options.end())
            throw runtime_error("option " + name + " already declared");
        m_options.emplace_back(new TypedOption<T>{*this, name, value});
        return *m_options.back();
    }
};

struct OptionManagerRegisterFuncs
{
    static void insert(OptionManager& options, OptionManagerWatcher& watcher)
    {
        options.register_watcher(watcher);
    }
    static void remove(OptionManager& options, OptionManagerWatcher& watcher)
    {
        options.unregister_watcher(watcher);
    }
};

class OptionManagerWatcher_AutoRegister
    : public OptionManagerWatcher,
      public AutoRegister<OptionManagerWatcher_AutoRegister,
                          OptionManagerRegisterFuncs, OptionManager>
{
public:
    OptionManagerWatcher_AutoRegister(OptionManager& options)
        : AutoRegister(options) {}
};


}

#endif // option_manager_hh_INCLUDED
