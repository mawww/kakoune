# http://gnu.org/software/screen/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group GNUscreen global KakBegin .* %sh{
    [ -z "${STY}" ] && exit
    echo "
        alias global focus screen-focus
        alias global new screen-new-vertical
        alias global terminal screen-terminal-vertical
    "
}

define-command screen-terminal-impl -hidden -params 3.. -command-completion %{
    nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval "$1" "$2"
        shift 2
        cmd=$(printf %s "$*")
        screen -X screen sh -c "${cmd}; screen -X remove" < "/dev/$tty"
    }
}

define-command screen-terminal-vertical -params 1.. -command-completion -docstring '
screen-terminal-vertical <program> [<arguments>]: create a new terminal as a screen pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal' \
%{
    screen-terminal-impl 'split -v' 'focus right' %arg{@}
}
define-command screen-terminal-horizontal -params 1.. -command-completion -docstring '
screen-terminal-horizontal <program> [<arguments>]: create a new terminal as a screen pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal' \
%{
    screen-terminal-impl 'split -h' 'focus down' %arg{@}
}
define-command screen-terminal-window -params 1.. -command-completion -docstring '
screen-terminal-window <program> [<arguments>]: create a new terminal as a screen window
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X screen sh -c "$*" < "/dev/$tty"
    }
}

define-command screen-new-vertical -params .. -command-completion -docstring '
screen-new-vertical [<commands>]: create a new kakoune client as a screen pane
The current pane is split into two, left and right
The optional arguments are passed as commands to the new client' \
%{
    screen-terminal-vertical "kak -c '%val{session}' -e '%arg{@}'"
}
define-command screen-new-horizontal -params .. -command-completion -docstring '
screen-new-horizontal [<commands>]: create a new kakoune client as a screen pane
The current pane is split into two, top and bottom
The optional arguments are passed as commands to the new client' \
%{
    screen-terminal-horizontal "kak -c '%val{session}' -e '%arg{@}'"
}
define-command screen-new-window -params .. -command-completion -docstring '
screen-new-window [<commands>]: create a new kakoune client as a screen window
The optional arguments are passed as commands to the new client' \
%{
    screen-terminal-window "kak -c '%val{session}' -e '%arg{@}'"
}

define-command screen-focus -params ..1 -client-completion -docstring '
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
