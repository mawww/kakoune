#ifndef function_registry_h_INCLUDED
#define function_registry_h_INCLUDED

#include "completion.hh"
#include "id_map.hh"
#include "string.hh"
#include "exception.hh"

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
    void register_func(StringView name, const FunctionType& function)
    {
        kak_assert(not m_functions.contains(name));
        m_functions.append(std::make_pair(name, function));
    }

    const FunctionType& operator[](StringView name) const
    {
        auto it = m_functions.find(name);
        if (it == m_functions.end())
            throw function_not_found(name);
        return it->second;
    }

    CandidateList complete_name(StringView prefix, ByteCount cursor_pos)
    {
        return m_functions.complete_id(prefix, cursor_pos);
    }

private:
    id_map<FunctionType> m_functions;
};

}

#endif // function_registry_h_INCLUDED
