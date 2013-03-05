#include "option_manager.hh"
#include "assert.hh"

#include <sstream>

namespace Kakoune
{

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

    String get_as_string() const override;
    void   set_from_string(const String& str) override;

    Option* clone(OptionManager& manager) const override
    {
        return new TypedOption{manager, name(), m_value};
    }
private:
    T m_value;
};

Option::Option(OptionManager& manager, String name)
    : m_manager(manager), m_name(std::move(name)) {}

template<typename T> const T& Option::get() const { return dynamic_cast<const TypedOption<T>*>(this)->get(); }
template<typename T> void     Option::set(const T& val) { return dynamic_cast<TypedOption<T>*>(this)->set(val); }

// TypedOption<String> specializations;
template<> String TypedOption<String>::get_as_string() const { return m_value; }
template<> void   TypedOption<String>::set_from_string(const String& str) { set(str); }
template class TypedOption<String>;
template const String& Option::get<String>() const;
template void Option::set<String>(const String&);

// TypedOption<int> specializations;
template<> String TypedOption<int>::get_as_string() const { return int_to_str(m_value); }
template<> void   TypedOption<int>::set_from_string(const String& str) { set(str_to_int(str)); }
template class TypedOption<int>;
template const int& Option::get<int>() const;
template void Option::set<int>(const int&);

// TypedOption<bool> specializations;
template<> String TypedOption<bool>::get_as_string() const { return m_value ? "true" : "false"; }
template<> void   TypedOption<bool>::set_from_string(const String& str)
{
    if (str == "true" or str == "yes")
        m_value = true;
    else if (str == "false" or str == "no")
        m_value = false;
    else
        throw runtime_error("boolean values are either true, yes, false or no");
}
template class TypedOption<bool>;
template const bool& Option::get<bool>() const;
template void Option::set<bool>(const bool&);

OptionManager::OptionManager(OptionManager& parent)
    : m_parent(&parent)
{
    parent.register_watcher(*this);
}

OptionManager::~OptionManager()
{
    if (m_parent)
        m_parent->unregister_watcher(*this);

    assert(m_watchers.empty());
}

void OptionManager::register_watcher(OptionManagerWatcher& watcher)
{
    assert(not contains(m_watchers, &watcher));
    m_watchers.push_back(&watcher);
}

void OptionManager::unregister_watcher(OptionManagerWatcher& watcher)
{
    auto it = find(m_watchers.begin(), m_watchers.end(), &watcher);
    assert(it != m_watchers.end());
    m_watchers.erase(it);
}

template<typename T>
auto find_option(T& container, const String& name) -> decltype(container.begin())
{
    using ptr_type = decltype(*container.begin());
    return find_if(container, [&name](const ptr_type& opt) { return opt->name() == name; });
}

Option& OptionManager::get_local_option(const String& name)
{
    auto it = find_option(m_options, name);
    if (it != m_options.end())
        return **it;
    else if (m_parent)
    {
        m_options.emplace_back((*m_parent)[name].clone(*this));
        return *m_options.back();
    }
    else
        throw option_not_found(name);

}

const Option& OptionManager::operator[](const String& name) const
{
    auto it = find_option(m_options, name);
    if (it != m_options.end())
        return **it;
    else if (m_parent)
        return (*m_parent)[name];
    else
        throw option_not_found(name);
}

CandidateList OptionManager::complete_option_name(const String& prefix,
                                                  ByteCount cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    if (m_parent)
        result = m_parent->complete_option_name(prefix, cursor_pos);
    for (auto& option : m_options)
    {
        const auto& name = option->name();
        if (name.substr(0, real_prefix.length()) == real_prefix and
            not contains(result, name))
            result.push_back(name);
    }
    return result;
}

OptionManager::OptionList OptionManager::flatten_options() const
{
    OptionList res = m_parent ? m_parent->flatten_options() : OptionList{};
    for (auto& option : m_options)
    {
        auto it = find_option(res, option->name());
        if (it != res.end())
            *it = option.get();
        else
            res.emplace_back(option.get());
    }
    return res;
}

void OptionManager::on_option_changed(const Option& option)
{
    // if parent option changed, but we overrided it, it's like nothing happened
    if (&option.manager() != this and
        find_option(m_options, option.name()) != m_options.end())
        return;

    for (auto watcher : m_watchers)
        watcher->on_option_changed(option);
}

GlobalOptions::GlobalOptions()
    : OptionManager()
{
    declare_option<int>("tabstop", 8);
    declare_option<int>("indentwidth", 4);
    declare_option<String>("eolformat", "lf");
    declare_option<String>("BOM", "no");
    declare_option<String>("shell", "sh");
    declare_option<bool>("complete_prefix", true);
    declare_option<bool>("incsearch", true);
    declare_option<String>("ignored_files", R"(^(\..*|.*\.(o|so|a)))$)");
    declare_option<String>("filetype", "");
}

template<typename T>
Option& GlobalOptions::declare_option(const String& name, const T& value)
{
    if (find_option(m_options, name) != m_options.end())
        throw runtime_error("option " + name + " already declared");
    m_options.emplace_back(new TypedOption<T>{*this, name, value});
    return *m_options.back();
}

}
