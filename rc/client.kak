decl str termcmd %sh{
    if [ -n "$TMUX" ]; then
        echo "'tmux split-window -h'"
    else
        echo "'urxvt -e sh -c'"
    fi
}

def -docstring 'create a new kak client for current session' \
    -shell-params \
    new %{ nop %sh{
    if [ $# -ne 0 ]; then kakoune_params="-e '$@'"; fi
    setsid ${kak_opt_termcmd} "kak -c ${kak_session} ${kakoune_params}" < /dev/null > /dev/null 2>&1 &
}}

def -docstring 'focus given client' \
    -shell-params -client-completion \
    focus %{ %sh{
    if [ $# -gt 1 ]; then
        echo "echo -color Error 'too many arguments, use focus [client]'"
    elif [ $# -eq 1 ]; then
        echo "eval -client '$1' focus"
    else
        if [ -n "$kak_client_env_TMUX" ]; then
            TMUX="$kak_client_env_TMUX" tmux select-pane -t "$kak_client_env_TMUX_PANE" > /dev/null
        else
            xdotool windowactivate $kak_client_env_WINDOWID > /dev/null
        fi
    fi
} }
