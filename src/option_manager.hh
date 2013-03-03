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

class GlobalOptions : public OptionManager,
                      public Singleton<GlobalOptions>
{
public:
    GlobalOptions();

    template<typename T>
    Option& declare_option(const String& name, const T& inital_value);
};

}

#endif // option_manager_hh_INCLUDED
