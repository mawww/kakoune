decl str termcmd %sh{
    if [ -n "$TMUX" ]; then
        echo "'tmux split-window -h'"
    else
        echo "'urxvt -e sh -c'"
    fi
}

def new -docstring 'create a new kak client for current session' \
        -shell-params %{ nop %sh{
    if [ $# -ne 0 ]; then kakoune_params="-e '$@'"; fi
    setsid ${kak_opt_termcmd} "kak -c ${kak_session} ${kakoune_params}" < /dev/null > /dev/null 2>&1 &
}}
