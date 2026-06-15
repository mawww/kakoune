#ifndef local_scope_hh_INCLUDED
#define local_scope_hh_INCLUDED

#include "scope.hh"
#include "context.hh"

namespace Kakoune
{

struct LocalScope : Scope
{
    LocalScope(Context& context)
        : Scope(context.scope()), m_context{context}
    {
        m_context.m_local_scopes.push_back(this);
    }

    ~LocalScope()
    {
        kak_assert(not m_context.m_local_scopes.empty() and m_context.m_local_scopes.back() == this);
        m_context.m_local_scopes.pop_back();
    }

private:
    Context& m_context;
};

}

#endif // local_scope_hh_INCLUDED
