#ifndef hook_manager_hh_INCLUDED
#define hook_manager_hh_INCLUDED

#include "completion.hh"
#include "safe_ptr.hh"
#include "meta.hh"
#include "enum.hh"
#include "array.hh"
#include "unique_ptr.hh"

namespace Kakoune
{

class Context;
class Regex;

enum class Hook
{
    BufCreate,
    BufNewFile,
    BufOpenFile,
    BufClose,
    BufWritePost,
    BufReload,
    BufWritePre,
    BufOpenFifo,
    BufCloseFifo,
    BufReadFifo,
    BufSetOption,
    ClientCreate,
    ClientClose,
    ClientRenamed,
    SessionRenamed,
    InsertChar,
    InsertDelete,
    InsertIdle,
    InsertKey,
    InsertMove,
    InsertCompletionHide,
    InsertCompletionShow,
    KakBegin,
    KakEnd,
    FocusIn,
    FocusOut,
    GlobalSetOption,
    RuntimeError,
    PromptIdle,
    NormalIdle,
    NextKeyIdle,
    NormalKey,
    ModeChange,
    EnterDirectory,
    RawKey,
    RegisterModified,
    WinClose,
    WinCreate,
    WinDisplay,
    WinResize,
    WinSetOption,
    ModuleLoaded,
    User
};

constexpr auto enum_desc(Meta::Type<Hook>)
{
    return make_array<EnumDesc<Hook>>({
        {Hook::BufCreate, "BufCreate"},
        {Hook::BufNewFile, "BufNewFile"},
        {Hook::BufOpenFile, "BufOpenFile"},
        {Hook::BufClose, "BufClose"},
        {Hook::BufWritePost, "BufWritePost"},
        {Hook::BufReload, "BufReload"},
        {Hook::BufWritePre, "BufWritePre"},
        {Hook::BufOpenFifo, "BufOpenFifo"},
        {Hook::BufCloseFifo, "BufCloseFifo"},
        {Hook::BufReadFifo, "BufReadFifo"},
        {Hook::BufSetOption, "BufSetOption"},
        {Hook::ClientCreate, "ClientCreate"},
        {Hook::ClientClose, "ClientClose"},
        {Hook::ClientRenamed, "ClientRenamed"},
        {Hook::SessionRenamed, "SessionRenamed"},
        {Hook::InsertChar, "InsertChar"},
        {Hook::InsertDelete, "InsertDelete"},
        {Hook::InsertIdle, "InsertIdle"},
        {Hook::InsertKey, "InsertKey"},
        {Hook::InsertMove, "InsertMove"},
        {Hook::InsertCompletionHide, "InsertCompletionHide"},
        {Hook::InsertCompletionShow, "InsertCompletionShow"},
        {Hook::KakBegin, "KakBegin"},
        {Hook::KakEnd, "KakEnd"},
        {Hook::FocusIn, "FocusIn"},
        {Hook::FocusOut, "FocusOut"},
        {Hook::GlobalSetOption, "GlobalSetOption"},
        {Hook::RuntimeError, "RuntimeError"},
        {Hook::PromptIdle, "PromptIdle"},
        {Hook::NormalIdle, "NormalIdle"},
        {Hook::NextKeyIdle, "NextKeyIdle"},
        {Hook::NormalKey, "NormalKey"},
        {Hook::ModeChange, "ModeChange"},
        {Hook::EnterDirectory, "EnterDirectory"},
        {Hook::RawKey, "RawKey"},
        {Hook::RegisterModified, "RegisterModified"},
        {Hook::WinClose, "WinClose"},
        {Hook::WinCreate, "WinCreate"},
        {Hook::WinDisplay, "WinDisplay"},
        {Hook::WinResize, "WinResize"},
        {Hook::WinSetOption, "WinSetOption"},
        {Hook::ModuleLoaded, "ModuleLoaded"},
        {Hook::User, "User"}
    });
}

enum class HookFlags
{
    None = 0,
    Always = 1 << 0,
    Once = 1 << 1
};
constexpr bool with_bit_ops(Meta::Type<HookFlags>) { return true; }

class HookManager : public SafeCountable
{
public:
    HookManager(HookManager& parent);
    ~HookManager();

    void reparent(HookManager& parent) { m_parent = &parent; }

    void add_hook(Hook hook, String group, HookFlags flags,
                  Regex filter, String commands, Context& context);
    void remove_hooks(const Regex& regex);
    CandidateList complete_hook_group(StringView prefix, ByteCount pos_in_token);
    void run_hook(Hook hook, StringView param, Context& context);

private:
    struct HookData;

    HookManager();
    // the only one allowed to construct a root hook manager
    friend class Scope;

    SafePtr<HookManager> m_parent;
    Array<Vector<UniquePtr<HookData>, MemoryDomain::Hooks>, enum_desc(Meta::Type<Hook>{}).size()> m_hooks;

    mutable Vector<std::pair<Hook, StringView>, MemoryDomain::Hooks> m_running_hooks;
    mutable Vector<UniquePtr<HookData>, MemoryDomain::Hooks> m_hooks_trash;
};

}

#endif // hook_manager_hh_INCLUDED
