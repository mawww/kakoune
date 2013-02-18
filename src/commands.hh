#ifndef commands_hh_INCLUDED
#define commands_hh_INCLUDED

#include "keys.hh"

namespace Kakoune
{

struct Context;

void register_commands();
void exec_keys(const KeyList& keys, Context& context);

}

#endif // commands_hh_INCLUDED

