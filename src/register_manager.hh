#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "register.hh"
#include "utils.hh"
#include "unordered_map.hh"

#include <vector>
#include <functional>

namespace Kakoune
{

using RegisterRetriever = std::function<std::vector<String> (const Context&)>;

class RegisterManager : public Singleton<RegisterManager>
{
public:
    Register& operator[](StringView reg);
    Register& operator[](Codepoint c);
    void register_dynamic_register(char reg, RegisterRetriever function);

protected:
    UnorderedMap<char, std::unique_ptr<Register>> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
