#ifndef option_manager_hh_INCLUDED
#define option_manager_hh_INCLUDED

#include "utils.hh"
#include "exception.hh"
#include "completion.hh"

#include <unordered_map>

namespace Kakoune
{

struct option_not_found : public runtime_error
{
    option_not_found(const String& name)
        : runtime_error("option not found: " + name) {}
};

String int_to_str(int value);

class Option
{
public:
    Option() {}
    explicit Option(int value) : m_value(int_to_str(value)) {}
    explicit Option(const String& value) : m_value(value) {}

    Option& operator=(int value) { m_value = int_to_str(value); return *this; }
    Option& operator=(const String& value) { m_value = value; return *this; }

    int    as_int()    const  { return atoi(m_value.c_str()); }
    String as_string() const { return m_value; }
private:
    String m_value;
};

class OptionManager
{
public:
    OptionManager(OptionManager& parent)
        : m_parent(&parent) {}

    Option& operator[] (const String& name);
    const Option& operator[] (const String& name) const;

    CandidateList complete_option_name(const String& prefix,
                                       size_t cursor_pos);

private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class GlobalOptionManager;

    std::unordered_map<String, Option> m_options;
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
