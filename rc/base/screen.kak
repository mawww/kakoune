# http://gnu.org/software/screen/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
 
%sh{
    if ! command -v screen >/dev/null; then
        echo 'echo -debug screen: warning, command dependency unmet: screen'
    fi
}

hook global KakBegin .* %{
    %sh{
        [ -z "${kak_client_env_STY}" ] && exit
        echo "
            alias global focus screen-focus
            alias global new screen-new-vertical
        "
    }
}


define-command screen-new-vertical -params .. -command-completion -docstring "Create a new vertical region" %{
     %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval 'split -h' 'focus down' "screen kak -c \"${kak_session}\" -e \"$*\"" < "/dev/$tty"
    }
}

define-command screen-new-horizontal -params .. -command-completion -docstring "Create a new horizontal region" %{
     %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval 'split -v' 'focus right' "screen kak -c \"${kak_session}\" -e \"$*\"" < "/dev/$tty"
    }
}

define-command screen-new-window -params .. -command-completion -docstring "Create a new window" %{
    %sh{
        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X screen kak -c "${kak_session}" -e "$*" < "/dev/$tty"
    }
}

define-command -docstring %{screen-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    -params ..1 -client-completion \
    screen-focus %{ %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "
                evaluate-commands -client '$1' %{ %sh{
                    screen -X focus
            }}"
        elif [ -n "${kak_client_env_STY}" ]; then
            tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
            screen -X select "$kak_client_env_WINDOW" < "/dev/$tty"
        fi
} }
