# termcmd should be set such as the next argument is the whole
# command line to execute
decl str termcmd %sh{
    if [ -n "$TMUX" ]; then
        echo "'tmux split-window -h'"
    else
        for terminal in urxvt rxvt xterm roxterm mintty; do
            if which $terminal > /dev/null 2>&1; then
                        echo "'$terminal -e sh -c'"
                exit
            fi
        done
        for terminal in gnome-terminal xfce4-terminal; do
            if which $terminal > /dev/null 2>&1; then
                        echo "'$terminal -e'"
                exit
            fi
        done
    fi
}

def -docstring 'create a new kak client for current session' \
    -shell-params \
    new %{ %sh{
            if [ -z "${kak_opt_termcmd}" ]; then
               echo "echo -color Error 'termcmd option is not set'"
               exit
            fi
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
