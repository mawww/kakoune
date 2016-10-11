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
            printf %s\\n "'$termcmd'"
            exit
        fi
    done
}

def -docstring %{x11-new [<command>]: create a new kak client for the current session
The optional arguments will be passed as arguments to the new client} \
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

def -docstring %{x11-focus [<client>]: focus a given client's window
If no client is passed, then the current client is used} \
    -params ..1 -client-completion \
    x11-focus %{ %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "eval -client '$1' focus"
        else
            xdotool windowactivate $kak_client_env_WINDOWID > /dev/null
        fi
} }

alias global focus x11-focus
alias global new x11-new
