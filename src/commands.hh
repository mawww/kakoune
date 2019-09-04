#ifndef commands_hh_INCLUDED
#define commands_hh_INCLUDED

#include "parameters_parser.hh"
#include "shell_manager.hh"

namespace Kakoune
{

void register_commands();

struct kill_session
{
    int exit_status;
};

template<bool next>
void cycle_buffer(const ParametersParser& parser, Context& context, const ShellContext&);

}

#endif // commands_hh_INCLUDED
