#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "register.hh"
#include "utils.hh"

namespace Kakoune
{

class RegisterManager : public Singleton<RegisterManager>
{
public:
    Register& operator[](char reg) { return m_registers[reg]; }

protected:
    std::unordered_map<char, Register> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
