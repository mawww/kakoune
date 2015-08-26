#ifndef commands_hh_INCLUDED
#define commands_hh_INCLUDED

#include "array_view.hh"

namespace Kakoune
{

class Context;
class Key;

void register_commands();
void exec_keys(ConstArrayView<Key> keys, Context& context);

struct kill_session {};

}

#endif // commands_hh_INCLUDED
