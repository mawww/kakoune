#ifndef register_manager_hh_INCLUDED
#define register_manager_hh_INCLUDED

#include "utils.hh"

namespace Kakoune
{

class RegisterManager : public Singleton<RegisterManager>
{
public:
    std::string& operator[](char reg) { return m_registers[reg]; }

protected:
    std::unordered_map<char, std::string> m_registers;
};

}

#endif // register_manager_hh_INCLUDED
