#ifndef function_registry_h_INCLUDED
#define function_registry_h_INCLUDED

#include "string.hh"
#include "completion.hh"
#include "idvaluemap.hh"

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
        assert(not m_functions.contains(name));
        m_functions.append(std::make_pair(name, function));
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
        return m_functions.complete_id(prefix, cursor_pos);
    }

private:
    idvaluemap<String, FunctionType> m_functions;
};

}

#endif // function_registry_h_INCLUDED
