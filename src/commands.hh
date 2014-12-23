#ifndef commands_hh_INCLUDED
#define commands_hh_INCLUDED

#include "keys.hh"
#include "memoryview.hh"

namespace Kakoune
{

class Context;

void register_commands();
void exec_keys(memoryview<Key> keys, Context& context);

}

#endif // commands_hh_INCLUDED
