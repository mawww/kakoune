#ifndef scope_hh_INCLUDED
#define scope_hh_INCLUDED

#include "alias_registry.hh"
#include "face_registry.hh"
#include "highlighter_group.hh"
#include "hook_manager.hh"
#include "keymap_manager.hh"
#include "option_manager.hh"
#include "utils.hh"

namespace Kakoune
{

class SharedScope;

class Scope
{
public:
    Scope(Scope& parent)
        : m_options(parent.options()),
          m_hooks(parent.hooks()),
          m_keymaps(parent.keymaps()),
          m_aliases(parent.aliases()),
          m_faces(parent.faces()),
          m_highlighters(parent.highlighters()) {}

    OptionManager&       options()            { return m_options; }
    const OptionManager& options()      const { return m_options; }
    HookManager&         hooks()              { return m_hooks; }
    const HookManager&   hooks()        const { return m_hooks; }
    KeymapManager&       keymaps()            { return m_keymaps; }
    const KeymapManager& keymaps()      const { return m_keymaps; }
    AliasRegistry&       aliases()            { return m_aliases; }
    const AliasRegistry& aliases()      const { return m_aliases; }
    FaceRegistry&        faces()              { return m_faces; }
    const FaceRegistry&  faces()        const { return m_faces; }
    Highlighters&        highlighters()       { return m_highlighters; }
    const Highlighters&  highlighters() const { return m_highlighters; }

	void add_linked(SharedScope& scope, Context& context);
	void remove_linked(SharedScope& scope, Context& context);
	Completions complete_linked(StringView name, ByteCount cursor_pos);

private:
    friend class GlobalScope;
    friend class SharedScope;
    Scope() = default;

    Vector<SafePtr<SharedScope>> m_linked;

    OptionManager m_options;
    HookManager   m_hooks;
    KeymapManager m_keymaps;
    AliasRegistry m_aliases;
    FaceRegistry  m_faces;
    Highlighters  m_highlighters;
};

class GlobalScope : public Scope, public OptionManagerWatcher, public Singleton<GlobalScope>
{
public:
    GlobalScope();
    ~GlobalScope();

    OptionsRegistry& option_registry() { return m_option_registry; }
    const OptionsRegistry& option_registry() const { return m_option_registry; }
private:
    void on_option_changed(const Option& option) override;

    OptionsRegistry m_option_registry;
};

struct scope_not_found : public runtime_error
{
    using runtime_error::runtime_error;
};

class SharedScope : public Scope, public SafeCountable
{
public:
    StringView get_name() const { return m_name; }
    SharedScope(String name) : m_name(std::move(name)) {}
private:
    String m_name;
};

class SharedScopeManager : public Singleton<SharedScopeManager> 
{
public:
    SharedScopeManager() = default;
    ~SharedScopeManager() = default;

    void register_scope(String name);
    void remove_scope(StringView id);
    SharedScope& get_scope(StringView id);
    SharedScope* get_scope_ifp(StringView id);
    Completions complete_scope(StringView id, ByteCount cursor_pos) const;
private:
	using ScopeMap = HashMap<String, std::unique_ptr<SharedScope>, MemoryDomain::Undefined>;
	ScopeMap m_scopes;
};

}

#endif // scope_hh_INCLUDED
