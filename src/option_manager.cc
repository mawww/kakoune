#include "option_manager.hh"

#include <sstream>

namespace Kakoune
{

std::string int_to_str(int value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

GlobalOptionManager::GlobalOptionManager()
    : OptionManager()
{
    (*this)["tabstop"] = 8;
}

}
