# http://gnu.org/software/screen/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 
hook -group GNUscreen global KakBegin .* %sh{
    [ -z "${STY}" ] && exit
    echo "
        alias global focus screen-focus
        alias global new screen-new-vertical
    "
}


define-command screen-new-vertical -params .. -command-completion -docstring "Split the current pane into two, left and right" %{
     nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval \
            'split -h' \
            'focus down' \
            "screen sh -c 'kak -c \"${kak_session}\" -e \"$*\" ;
                screen -X remove'" \
        < "/dev/$tty"
    }
}

define-command screen-new-horizontal -params .. -command-completion -docstring "Split the current pane into two, top and bottom" %{
     nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval \
            'split -v' \
            'focus right' \
            "screen sh -c 'kak -c \"${kak_session}\" -e \"$*\" ;
                screen -X remove'" \
        < "/dev/$tty"
    }
}

define-command screen-new-window -params .. -command-completion -docstring "Create a new window" %{
    nop %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X screen kak -c "${kak_session}" -e "$*" < "/dev/$tty"
    }
}

define-command -docstring %{screen-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    -params ..1 -client-completion \
    screen-focus %{ evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "
                evaluate-commands -client '$1' focus
                "
        elif [ -n "${kak_client_env_STY}" ]; then
            tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
            screen -X select "${kak_client_env_WINDOW}" < "/dev/$tty"
        fi
} }
