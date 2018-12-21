# termcmd should be set such as the next argument is the whole
# command line to execute
declare-option -docstring %{shell command run to spawn a new terminal
A shell command is appended to the one set in this option at runtime} \
    str termcmd %sh{
    for termcmd in 'alacritty      -e sh -c' \
                   'kitty             sh -c' \
                   'termite        -e      ' \
                   'urxvt          -e sh -c' \
                   'rxvt           -e sh -c' \
                   'xterm          -e sh -c' \
                   'roxterm        -e sh -c' \
                   'mintty         -e sh -c' \
                   'sakura         -x      ' \
                   'gnome-terminal -e      ' \
                   'xfce4-terminal -e      ' ; do
        terminal=${termcmd%% *}
        if command -v $terminal >/dev/null 2>&1; then
            printf %s\\n "$termcmd"
            exit
        fi
    done
}

define-command x11-terminal -params 1.. -shell-completion -docstring '
x11-terminal <program> [<arguments>]: create a new terminal as an x11 window
The program passed as argument will be executed in the new terminal' \
%{
    evaluate-commands %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo "fail 'termcmd option is not set'"
           exit
        fi
        # join arguments into a single string, in which they're delimited
        # by single quotes, and with single quotes inside transformed to '\''
        # so that sh -c "$args" will re-split the arguments properly
        # example:
        # $1 = ab
        # $2 = foo bar
        # $3 =
        # $4 = foo'bar
        # $args = 'ab' 'foo bar' '' 'foo'\''bar'
        # would be nicer to do in a single sed/awk call but that's difficult
        args=$(
            for i in "$@"; do
                # special case to preserve empty variables as sed won't touch these
                if [ "$i" = '' ]; then
                    printf "'' "
                else
                    printf %s "$i" | sed -e "s|'|'\\\\''|g; s|^|'|; s|$|' |"
                fi
            done
        )
        setsid ${kak_opt_termcmd} "$args" < /dev/null > /dev/null 2>&1 &
    }
}

define-command x11-focus -params ..1 -client-completion -docstring '
x11-focus [<kakoune_client>]: focus a given client''s window
If no client is passed, then the current client is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' focus" "$1"
        else
            xdotool windowactivate $kak_client_env_WINDOWID > /dev/null
        fi
    }
}

alias global focus x11-focus
alias global terminal x11-terminal
