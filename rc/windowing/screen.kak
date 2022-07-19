# http://gnu.org/software/screen/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾


provide-module screen %{

# ensure that we're running under screen
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "$STY" ] || echo 'fail screen not detected'
}

define-command screen-terminal-impl -hidden -params 3.. %{
    nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval "$1" "$2"
        shift 2
        # see x11.kak for what this achieves
        args=$(
            for i in "$@"; do
                if [ "$i" = '' ]; then
                    printf "'' "
                else
                    printf %s "$i" | sed -e "s|'|'\\\\''|g; s|^|'|; s|$|' |"
                fi
            done
        )
        screen -X screen sh -c "${args} ; screen -X remove" < "/dev/$tty"
    }
}

define-command screen-terminal-vertical -params 1.. -docstring '
screen-terminal-vertical <program> [<arguments>] [<arguments>]: create a new terminal as a screen pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal' \
%{
    screen-terminal-impl 'split -v' 'focus right' %arg{@}
}
complete-command screen-terminal-vertical shell

define-command screen-terminal-horizontal -params 1.. -docstring '
screen-terminal-horizontal <program> [<arguments>]: create a new terminal as a screen pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal' \
%{
    screen-terminal-impl 'split -h' 'focus down' %arg{@}
}
complete-command screen-terminal-horizontal shell

define-command screen-terminal-window -params 1.. -docstring '
screen-terminal-window <program> [<arguments>]: create a new terminal as a screen window
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X screen "$@" < "/dev/$tty"
    }
}
complete-command screen-terminal-window shell

define-command screen-focus -params ..1 -docstring '
screen-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "
                evaluate-commands -client '$1' focus
                "
        elif [ -n "${kak_client_env_STY}" ]; then
            tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
            screen -X select "${kak_client_env_WINDOW}" < "/dev/$tty"
        fi
    }
}
complete-command -menu screen-focus client 

alias global focus screen-focus
alias global terminal screen-terminal-vertical

}
