# termcmd should be set such as the next argument is the whole
# command line to execute
decl str termcmd %sh{
    for termcmd in 'termite        -e      ' \
                   'urxvt          -e sh -c' \
                   'rxvt           -e sh -c' \
                   'xterm          -e sh -c' \
                   'roxterm        -e sh -c' \
                   'mintty         -e sh -c' \
                   'sakura         -e      ' \
                   'gnome-terminal -e      ' \
                   'xfce4-terminal -e      ' ; do
        terminal=${termcmd%% *}
        if which $terminal > /dev/null 2>&1; then
            printf %s "'$termcmd'"
            exit
        fi
    done
}

def -docstring 'create a new kak client for current session' \
    -params .. \
    -command-completion \
    x11-new %{ %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo "echo -color Error 'termcmd option is not set'"
           exit
        fi
        if [ $# -ne 0 ]; then kakoune_params="-e '$@'"; fi
        setsid ${kak_opt_termcmd} "kak -c ${kak_session} ${kakoune_params}" < /dev/null > /dev/null 2>&1 &
}}

def -docstring 'create a new window for repl interaction' \
    -params 0..1 \
    -command-completion \
    x11-repl %{ %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo "echo -color Error 'termcmd option is not set'"
           exit
        fi
        if [ $# -eq 0 ]; then cmd="bash"; else cmd="$1"; fi
        setsid ${kak_opt_termcmd} ${cmd} -t kak_repl_window < /dev/null > /dev/null 2>&1 &
}}

def x11-send-text -docstring "send selected text to the repl window" %{
    %sh{
        echo "${kak_selection}" | xsel -i
        wid=$(xdotool getactivewindow)
        xdotool search --name kak_repl_window windowactivate
        xdotool key --clearmodifiers "Shift+Insert"
        xdotool windowactivate $wid
    }
}

def -docstring 'focus given client\'s window' \
    -params 0..1 -client-completion \
    x11-focus %{ %sh{
        if [ $# -gt 1 ]; then
            echo "echo -color Error 'too many arguments, use focus [client]'"
        elif [ $# -eq 1 ]; then
            echo "eval -client '$1' focus"
        else
            xdotool windowactivate $kak_client_env_WINDOWID > /dev/null
        fi
} }

alias global focus x11-focus
alias global new x11-new
alias global repl x11-repl
alias global send-text x11-send-text
