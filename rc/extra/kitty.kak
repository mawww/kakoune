declare-option -docstring %{window type that kitty creates on new and repl calls (kitty|os)} str kitty_window_type kitty

hook -group kitty-hooks global KakBegin .* %sh{
    if [ "$TERM" = "xterm-kitty" ] && [ -z "$TMUX" ]; then
        echo "
            alias global terminal kitty-terminal
            alias global terminal-tab kitty-terminal-tab
            alias global focus kitty-focus
            alias global repl kitty-repl
            alias global send-text kitty-send-text
        "
    fi
}

define-command kitty-terminal -params 1.. -shell-completion -docstring '
kitty-terminal <program> [<arguments>]: create a new terminal as a kitty window
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        kitty @ new-window --no-response --window-type $kak_opt_kitty_window_type "$@"
    }
}

define-command kitty-terminal-tab -params 1.. -shell-completion -docstring '
kitty-terminal-tab <program> [<arguments>]: create a new terminal as kitty tab
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        kitty @ new-window --no-response --new-tab "$@"
    }
}

define-command kitty-focus -params ..1 -client-completion -docstring '
kitty-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' focus" "$1"
        else
            kitty @ focus-tab --no-response -m=id:$kak_client_env_KITTY_WINDOW_ID
            kitty @ focus-window --no-response -m=id:$kak_client_env_KITTY_WINDOW_ID
        fi
    }
}

define-command kitty-repl -params .. -shell-completion -docstring '
kitty-repl [<arguments>]: create a new window for repl interaction
All optional parameters are forwarded to the new window' \
%{
    nop %sh{
        if [ $# -eq 0 ]; then
            cmd="${SHELL:-/bin/sh}"
        else
            cmd="$*"
        fi
        kitty @ new-window --no-response --window-type $kak_opt_kitty_window_type --title kak_repl_window --cwd "$PWD" $cmd < /dev/null > /dev/null 2>&1 &
    }
}

define-command kitty-send-text -docstring "send the selected text to the repl window" %{
    nop %sh{
        kitty @ send-text -m=title:kak_repl_window "${kak_selection}"
    }
}
