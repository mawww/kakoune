decl str termcmd %sh{
    if [[ -n "$TMUX" ]]; then
        echo "'tmux split-window -h'"
    else
        echo "'urxvt -e sh -c'"
    fi
}

def new -shell-params %{ nop %sh{
    if (( $# != 0 )); then kakoune_params="-e '$@'"; fi
    ${kak_opt_termcmd} "kak -c ${kak_session} ${kakoune_params}" < /dev/null >& /dev/null &
    disown
}}
