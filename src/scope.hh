#ifndef scope_hh_INCLUDED
#define scope_hh_INCLUDED

#include "unique_ptr.hh"
#include "utils.hh"

namespace Kakoune
{

class AliasRegistry;
class FaceRegistry;
class Highlighters;
class HookManager;
class KeymapManager;
class OptionManager;
class OptionsRegistry;

class Scope
{
public:
    Scope(Scope& parent);
    ~Scope();

    OptionManager&       options();
    const OptionManager& options() const;
    HookManager&         hooks();
    const HookManager&   hooks() const;
    KeymapManager&       keymaps();
    const KeymapManager& keymaps() const;
    AliasRegistry&       aliases();
    const AliasRegistry& aliases() const;
    FaceRegistry&        faces();
    const FaceRegistry&  faces() const;
    Highlighters&        highlighters();
    const Highlighters&  highlighters() const;

    void reparent(Scope& parent);

private:
    friend class GlobalScope;
    Scope();
    struct Data;
    UniquePtr<Data> m_data;
};

class GlobalScope : public Scope, public Singleton<GlobalScope>
{
public:
    GlobalScope();
    ~GlobalScope();

    OptionsRegistry& option_registry();
    const OptionsRegistry& option_registry() const;

private:
    struct GlobalData;
    UniquePtr<GlobalData> m_global_data;
};

}

#endif // scope_hh_INCLUDED
