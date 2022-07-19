# wayland

provide-module wayland %{

# ensure that we're running in the right environment
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "$WAYLAND_DISPLAY" ] || echo 'fail WAYLAND_DISPLAY is not set'
}

# termcmd should be set such as the next argument is the whole
# command line to execute
declare-option -docstring %{shell command run to spawn a new terminal
A shell command is appended to the one set in this option at runtime} \
    str termcmd %sh{
    for termcmd in 'alacritty      -e sh -c' \
                   'kitty             sh -c' \
                   'foot              sh -c' \
                   'termite        -e      ' \
                   'wterm          -e sh -c' \
                   'gnome-terminal -e      ' \
                   'xfce4-terminal -e      ' \
                   'konsole        -e      '; do
        terminal=${termcmd%% *}
        if command -v $terminal >/dev/null 2>&1; then
            printf %s\\n "$termcmd"
            exit
        fi
    done
}

define-command wayland-terminal -params 1.. -docstring '
wayland-terminal <program> [<arguments>]: create a new terminal as a Wayland window
The program passed as argument will be executed in the new terminal' \
%{
    evaluate-commands -save-regs 'a' %{
        set-register a %arg{@}
        evaluate-commands %sh{
            if [ -z "${kak_opt_termcmd}" ]; then
                echo "fail 'termcmd option is not set'"
                exit
            fi
            setsid ${kak_opt_termcmd} "$kak_quoted_reg_a" < /dev/null > /dev/null 2>&1 &
        }
    }
}
complete-command wayland-terminal shell

define-command wayland-focus -params ..1 -docstring '
wayland-focus [<kakoune_client>]: focus a given client''s window
If no client is passed, then the current client is used' \
%{
    fail 'Focusing specific windows in most Wayland window managers is unsupported'
}
complete-command -menu wayland-focus client

alias global focus wayland-focus
alias global terminal wayland-terminal

}
