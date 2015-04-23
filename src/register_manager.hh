#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "array_view.hh"
#include "utils.hh"
#include "unordered_map.hh"
#include "string.hh"
#include "vector.hh"

#include <functional>

namespace Kakoune
{

class Context;

class Register
{
public:
    virtual ~Register() {}
    virtual Register& operator=(ConstArrayView<String> values) = 0;

    virtual ConstArrayView<String> values(const Context& context) = 0;
};

using RegisterRetriever = std::function<Vector<String, MemoryDomain::Registers> (const Context&)>;

class RegisterManager : public Singleton<RegisterManager>
{
public:
    Register& operator[](StringView reg);
    Register& operator[](Codepoint c);
    void register_dynamic_register(char reg, RegisterRetriever function);

protected:
    UnorderedMap<char, std::unique_ptr<Register>, MemoryDomain::Registers> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
