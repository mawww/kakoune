#ifndef option_manager_hh_INCLUDED
#define option_manager_hh_INCLUDED

#include "completion.hh"
#include "exception.hh"
#include "hash_map.hh"
#include "option.hh"
#include "ranges.hh"
#include "utils.hh"
#include "vector.hh"
#include "string_utils.hh"

#include <memory>
#include <utility>

namespace Kakoune
{

class OptionManager;
class Context;

enum class OptionFlags
{
    None   = 0,
    Hidden = 1,
};

constexpr bool with_bit_ops(Meta::Type<OptionFlags>) { return true; }

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

class Option : public UseMemoryDomain<MemoryDomain::Options>
{
public:
    virtual ~Option() = default;

    template<typename T> const T& get() const;
    template<typename T> T& get_mutable();
    template<typename T> void set(const T& val, bool notify=true);
    template<typename T> bool is_of_type() const;

    virtual String get_as_string(Quoting quoting) const = 0;
    virtual Vector<String> get_as_strings() const = 0;
    virtual String get_desc_string() const = 0;
    virtual void set_from_strings(ConstArrayView<String> strs) = 0;
    virtual void add_from_strings(ConstArrayView<String> strs) = 0;
    virtual void remove_from_strings(ConstArrayView<String> strs) = 0;
    virtual void update(const Context& context) = 0;

    virtual bool has_same_value(const Option& other) const = 0;

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
    virtual void on_option_changed(const Option& option) = 0;
};

class OptionManager final : private OptionManagerWatcher
{
public:
    OptionManager(OptionManager& parent);
    ~OptionManager();

    Option& operator[] (StringView name);
    const Option& operator[] (StringView name) const;
    Option& get_local_option(StringView name);

    void unset_option(StringView name);

    auto flatten_options() const
    {
        auto merge = [](auto&& first, const OptionMap& second) {
            return concatenated(std::forward<decltype(first)>(first)
                                | filter([&second](auto& i) { return not second.contains(i.key); }),
                                second);
        };
        static const OptionMap empty;
        auto& parent = m_parent ? m_parent->m_options : empty;
        auto& grand_parent = (m_parent and m_parent->m_parent) ? m_parent->m_parent->m_options : empty;
        return merge(merge(grand_parent, parent), m_options) | transform(&OptionMap::Item::value);
    }

    void register_watcher(OptionManagerWatcher& watcher) const;
    void unregister_watcher(OptionManagerWatcher& watcher) const;

    void on_option_changed(const Option& option) override;
private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class Scope;
    friend class OptionsRegistry;
    using OptionMap = HashMap<StringView, std::unique_ptr<Option>, MemoryDomain::Options>;

    OptionMap m_options;
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

    Vector<String> get_as_strings() const override
    {
        return option_to_strings(m_value);
    }

    String get_as_string(Quoting quoting) const override
    {
        return option_to_string(m_value, quoting);
    }

    String get_desc_string() const override
    {
        if constexpr (std::is_same_v<int, T> or std::is_same_v<bool, T> or std::is_same_v<String, T>)
            return option_to_string(m_value, Quoting::Raw);
        else
            return "...";
    }

    void set_from_strings(ConstArrayView<String> strs) override
    {
        set(option_from_strings(Meta::Type<T>{}, strs));
    }

    void add_from_strings(ConstArrayView<String> strs) override
    {
        if (option_add_from_strings(m_value, strs))
            m_manager.on_option_changed(*this);
    }

    void remove_from_strings(ConstArrayView<String> strs) override
    {
        if (option_remove_from_strings(m_value, strs))
            m_manager.on_option_changed(*this);
    }

    void update(const Context& context) override
    {
        option_update(m_value, context);
    }

    bool has_same_value(const Option& other) const override
    {
        return other.is_of_type<T>() and other.get<T>() == m_value;
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
        throw runtime_error(format("option '{}' is not of type '{}'", name(),
                                   option_type_name(Meta::Type<T>{})));
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
        throw runtime_error(format("option '{}' is not of type '{}'", name(),
                                   option_type_name(Meta::Type<T>{})));
    return typed_opt->set(val, notify);
}

template<typename T> bool Option::is_of_type() const
{
    return dynamic_cast<const TypedOption<T>*>(this) != nullptr;
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
        auto is_option_identifier = [](char c) {
            return is_basic_alpha(c) or is_basic_digit(c) or c == '_';
        };

        if (not all_of(name, is_option_identifier))
            throw runtime_error{format("name '{}' contains char out of [a-zA-Z0-9_]", name)};

        auto& opts = m_global_manager.m_options;
        auto it = opts.find(name);
        if (it != opts.end())
        {
            if (it->value->is_of_type<T>() and it->value->flags() == flags)
                return *it->value;
            throw runtime_error{format("option '{}' already declared with different type or flags", name)};
        }
        String doc =  docstring.empty() ? format("[{}]", option_type_name(Meta::Type<T>{}))
                                        : format("[{}] - {}", option_type_name(Meta::Type<T>{}), docstring);
        m_descs.emplace_back(new OptionDesc{name.str(), std::move(doc), flags});
        return *opts.insert({m_descs.back()->name(),
                             std::make_unique<TypedCheckedOption<T, validator>>(m_global_manager, *m_descs.back(), value)});
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

    void clear_option_trash() { m_option_trash.clear(); }
    void move_to_trash(std::unique_ptr<Option>&& option) { m_option_trash.push_back(std::move(option)); }
private:
    OptionManager& m_global_manager;
    Vector<std::unique_ptr<const OptionDesc>, MemoryDomain::Options> m_descs;
    Vector<std::unique_ptr<Option>> m_option_trash;
};

}

#endif // option_manager_hh_INCLUDED
