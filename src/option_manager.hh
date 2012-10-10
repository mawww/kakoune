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

    bool operator==(const Option& other) const { return m_value == other.m_value; }
    bool operator!=(const Option& other) const { return m_value != other.m_value; }

    int    as_int()    const  { return str_to_int(m_value); }
    String as_string() const { return m_value; }
private:
    String m_value;
};

class OptionManagerWatcher
{
public:
    virtual ~OptionManagerWatcher() {}

    virtual void on_option_changed(const String& name,
                                   const Option& option) = 0;
};

class OptionManager : private OptionManagerWatcher
{
public:
    OptionManager(OptionManager& parent);
    ~OptionManager();

    const Option& operator[] (const String& name) const;

    void set_option(const String& name, const Option& value);

    CandidateList complete_option_name(const String& prefix,
                                       ByteCount cursor_pos);

    typedef std::unordered_map<String, Option> OptionMap;
    OptionMap flatten_options() const;

    void register_watcher(OptionManagerWatcher& watcher);
    void unregister_watcher(OptionManagerWatcher& watcher);

private:
    OptionManager()
        : m_parent(nullptr) {}
    // the only one allowed to construct a root option manager
    friend class GlobalOptionManager;

    OptionMap m_options;
    OptionManager* m_parent;

    void on_option_changed(const String& name,
                           const Option& value);

    std::vector<OptionManagerWatcher*> m_watchers;
};

class GlobalOptionManager : public OptionManager,
                            public Singleton<GlobalOptionManager>
{
public:
    GlobalOptionManager();
};


}

#endif // option_manager_hh_INCLUDED
