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
        setsid ${kak_opt_termcmd} "$*" < /dev/null > /dev/null 2>&1 &
    }
}

define-command x11-new -params .. -command-completion -docstring '
x11-new [<commands>]: create a new kakoune client as an x11 window
The optional arguments are passed as commands to the new client' \
%{
    x11-terminal "kak -c %val{session} -e '%arg{@}'"
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
alias global new x11-new
alias global terminal x11-terminal
