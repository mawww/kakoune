# x11

provide-module x11 %{

# ensure that we're running in the right environment
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "$DISPLAY" ] || echo 'fail DISPLAY is not set'
}

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
                   'st             -e sh -c' \
                   'xterm          -e sh -c' \
                   'roxterm        -e sh -c' \
                   'mintty         -e sh -c' \
                   'sakura         -x      ' \
                   'gnome-terminal -e      ' \
                   'xfce4-terminal -x sh -c' \
                   'konsole        -e      '; do
        terminal=${termcmd%% *}
        if command -v $terminal >/dev/null 2>&1; then
            printf %s\\n "$termcmd"
            exit
        fi
    done
}

define-command x11-terminal-window -params 1.. -docstring '
x11-terminal-window <program> [<arguments>]: create a new terminal as an X11 window
The program passed as argument will be executed in the new terminal' \
%{
    evaluate-commands -save-regs 'a' %{
        set-register a %arg{@}
        evaluate-commands %sh{
            if [ -z "${kak_opt_termcmd}" ]; then
                echo "fail 'termcmd option is not set'"
                exit
            fi
            termcmd=$kak_opt_termcmd
            args=$kak_quoted_reg_a
            unset kak_opt_termcmd kak_quoted_reg_a
            setsid ${termcmd} "$args" < /dev/null > /dev/null 2>&1 &
        }
    }
}
complete-command x11-terminal-window shell 

define-command x11-focus -params ..1 -docstring '
x11-focus [<kakoune_client>]: focus a given client''s window
If no client is passed, then the current client is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' focus" "$1"
        else
            xdotool windowactivate $kak_client_env_WINDOWID > /dev/null ||
            echo 'fail failed to run x11-focus, see *debug* buffer for details'
        fi
    }
}
complete-command -menu x11-focus client 

alias global focus x11-focus

# deprecated
define-command -hidden x11-terminal -params 1.. %{
    x11-terminal-window %arg{@}
}

}
