#ifndef option_manager_hh_INCLUDED
#define option_manager_hh_INCLUDED

#include "utils.hh"
#include "exception.hh"

#include <unordered_map>

namespace Kakoune
{

struct option_not_found : public runtime_error
{
    option_not_found(const std::string& name)
        : runtime_error("option not found: " + name) {}
};

std::string int_to_str(int value);

class Option
{
public:
    Option() {}
    explicit Option(int value) : m_value(int_to_str(value)) {}
    explicit Option(const std::string& value) : m_value(value) {}

    Option& operator=(int value) { m_value = int_to_str(value); return *this; }
    Option& operator=(const std::string& value) { m_value = value; return *this; }

    operator int() const { return atoi(m_value.c_str()); }
    operator std::string() const { return m_value; }
private:
    std::string m_value;
};

class OptionManager
{
public:
    OptionManager(OptionManager& parent)
        : m_parent(&parent) {}

    Option& operator[] (const std::string& name)
    {
        auto it = m_options.find(name);
        if (it != m_options.end())
            return it->second;
        else if (m_parent)
            return (*m_parent)[name];
        else
            return m_options[name];
    }

    const Option& operator[] (const std::string& name) const
    {
        auto it = m_options.find(name);
        if (it != m_options.end())
            return it->second;
        else if (m_parent)
            return (*m_parent)[name];
        else
            throw option_not_found(name);
    }

private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class GlobalOptionManager;

    std::unordered_map<std::string, Option> m_options;
    OptionManager* m_parent;
};

class GlobalOptionManager : public OptionManager,
                            public Singleton<GlobalOptionManager>
{
public:
    GlobalOptionManager();
};


}

#endif // option_manager_hh_INCLUDED
