#ifndef function_group_hh_INCLUDED
#define function_group_hh_INCLUDED

#include "exception.hh"
#include "id_map.hh"
#include "string.hh"

namespace Kakoune
{

template<typename... Args>
class FunctionGroup
{
public:
    using Function = std::function<void (Args...)>;
    using FunctionAndId = std::pair<String, std::function<void (Args...)>>;

    void operator()(Args&&... args)
    {
        for (auto& func : m_functions)
           func.second(std::forward<Args>(args)...);
    }

    void append(FunctionAndId&& function)
    {
        if (m_functions.contains(function.first))
            throw runtime_error("duplicate id: " + function.first);

        m_functions.append(std::forward<FunctionAndId>(function));
    }
    void remove(const String& id)
    {
        m_functions.remove(id);
    }

    FunctionGroup& get_group(const String& id)
    {
        auto it = m_functions.find(id);
        if (it == m_functions.end())
            throw runtime_error("no such id: " + id);
        FunctionGroup* group = it->second.template target<FunctionGroup>();
        if (not group)
            throw runtime_error("not a group: " + id);
        return *group;
    }

    CandidateList complete_id(const String& prefix, ByteCount cursor_pos) const
    {
        return m_functions.complete_id(prefix, cursor_pos);
    }

    CandidateList complete_group_id(const String& prefix, ByteCount cursor_pos) const
    {
        return m_functions.complete_id_if(
            prefix, cursor_pos, [](const FunctionAndId& func) {
                return func.second.template target<FunctionGroup>() != nullptr;
            });
    }

private:
    id_map<Function> m_functions;
};

}

#endif // function_group_hh_INCLUDED
