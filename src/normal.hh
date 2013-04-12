#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "keys.hh"

#include <functional>
#include <unordered_map>

namespace Kakoune
{

class Context;

using KeyMap = std::unordered_map<Key, std::function<void (Context& context)>>;
extern KeyMap keymap;

}

#endif // normal_hh_INCLUDED
