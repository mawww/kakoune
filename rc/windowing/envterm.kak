provide-module envterm %{

# ensure that we're running on iTerm
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || ! which "$kak_env_TERM" || echo 'fail $TERM binary not found'
}

define-command envterm-terminal-window -params 1.. -docstring '
envterm-terminal-window <program> [<arguments>]: create a new terminal as an $TERM window
The program passed as argument will be executed in the new terminal'\
%{
    nop %sh{ "$TERM" -e "$@" > /dev/null 2>&1 & }
}

}

