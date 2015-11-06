# termcmd should be set such as the next argument is the whole
# command line to execute
decl str termcmd %sh{
    for termcmd in 'termite        -e      ' \
                   'urxvt          -e sh -c' \
                   'rxvt           -e sh -c' \
                   'xterm          -e sh -c' \
                   'roxterm        -e sh -c' \
                   'mintty         -e sh -c' \
                   'gnome-terminal -e      ' \
                   'xfce4-terminal -e      ' ; do
        terminal=${termcmd%% *}
        if which $terminal > /dev/null 2>&1; then
            echo "'$termcmd'"
            exit
        fi
    done
}

def -docstring 'create a new kak client for current session' \
    -shell-params \
    -command-completion \
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
    focus-default %{ %sh{
    if [ $# -gt 1 ]; then
        echo "echo -color Error 'too many arguments, use focus [client]'"
    elif [ $# -eq 1 ]; then
        echo "eval -client '$1' focus"
    else
        xdotool windowactivate $kak_client_env_WINDOWID > /dev/null
    fi
} }
## Create an alias to the default focus callback to allow overriding from custom scripts (i.e. tmux)
alias global focus focus-default
