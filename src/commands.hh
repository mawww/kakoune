#ifndef commands_hh_INCLUDED
#define commands_hh_INCLUDED

namespace Kakoune
{

void register_commands();

struct kill_session
{
    int exit_status;
};

}

#endif // commands_hh_INCLUDED
