#ifndef function_registry_h_INCLUDED
#define function_registry_h_INCLUDED

#include "completion.hh"
#include "string.hh"

#include <unordered_map>

namespace Kakoune
{

struct function_not_found : runtime_error
{
    function_not_found(const String& name)
        : runtime_error("'" + name + "' not found") {}
};

template<typename FunctionType>
class FunctionRegistry
{
public:
    void register_func(const String& name, const FunctionType& function)
    {
        kak_assert(m_functions.find(name) == m_functions.end());
        m_functions[name] = function;
    }

    const FunctionType& operator[](const String& name) const
    {
        auto it = m_functions.find(name);
        if (it == m_functions.end())
            throw function_not_found(name);
        return it->second;
    }

    CandidateList complete_name(const String& prefix, ByteCount cursor_pos)
    {
        return complete_key(m_functions, prefix, cursor_pos);
    }

private:
    std::unordered_map<String, FunctionType> m_functions;
};

}

#endif // function_registry_h_INCLUDED
