# https://sw.kovidgoyal.net/kitty/index.html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module kitty %{

# ensure that we're running on kitty
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ "$TERM" = "xterm-kitty" ] || echo 'fail Kitty not detected'
}

declare-option -docstring %{window type that kitty creates on new and repl calls (kitty|os)} str kitty_window_type kitty

define-command kitty-terminal -params 1.. -shell-completion -docstring '
kitty-terminal <program> [<arguments>]: create a new terminal as a kitty window
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        kitty @ new-window --no-response --window-type $kak_opt_kitty_window_type --cwd "$PWD" "$@"
    }
}

define-command kitty-terminal-tab -params 1.. -shell-completion -docstring '
kitty-terminal-tab <program> [<arguments>]: create a new terminal as kitty tab
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        kitty @ new-window --no-response --new-tab --cwd "$PWD" "$@"
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

alias global terminal kitty-terminal
alias global terminal-tab kitty-terminal-tab
alias global focus kitty-focus

}
