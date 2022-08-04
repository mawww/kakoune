# https://sw.kovidgoyal.net/kitty/index.html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module kitty %{

# ensure that we're running on kitty
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ "$TERM" = "xterm-kitty" ] || echo 'fail Kitty not detected'
}

declare-option -docstring %{window type that kitty creates on new and repl calls (window|os-window)} str kitty_window_type window

define-command kitty-terminal -params 1.. -docstring '
kitty-terminal <program> [<arguments>]: create a new terminal as a kitty window
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        match=""
        if [ -n "$kak_client_env_KITTY_WINDOW_ID" ]; then
            match="--match=id:$kak_client_env_KITTY_WINDOW_ID"
        fi

        listen=""
        if [ -n "$kak_client_env_KITTY_LISTEN_ON" ]; then
            listen="--to=$kak_client_env_KITTY_LISTEN_ON"
        fi

        kitty @ $listen launch --no-response --type="$kak_opt_kitty_window_type" --cwd="$PWD" $match "$@"
    }
}
complete-command kitty-terminal shell

define-command kitty-terminal-tab -params 1.. -docstring '
kitty-terminal-tab <program> [<arguments>]: create a new terminal as kitty tab
The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{
        match=""
        if [ -n "$kak_client_env_KITTY_WINDOW_ID" ]; then
            match="--match=id:$kak_client_env_KITTY_WINDOW_ID"
        fi

        listen=""
        if [ -n "$kak_client_env_KITTY_LISTEN_ON" ]; then
            listen="--to=$kak_client_env_KITTY_LISTEN_ON"
        fi

        kitty @ $listen launch --no-response --type=tab --cwd="$PWD" $match "$@"
    }
}
complete-command kitty-terminal-tab shell

define-command kitty-focus -params ..1 -docstring '
kitty-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' focus" "$1"
        else
            match=""
            if [ -n "$kak_client_env_KITTY_WINDOW_ID" ]; then
                match="--match=id:$kak_client_env_KITTY_WINDOW_ID"
            fi

            listen=""
            if [ -n "$kak_client_env_KITTY_LISTEN_ON" ]; then
                listen="--to=$kak_client_env_KITTY_LISTEN_ON"
            fi

            kitty @ $listen focus-window --no-response $match
        fi
    }
}
complete-command -menu kitty-focus client

alias global terminal kitty-terminal
alias global terminal-tab kitty-terminal-tab
alias global focus kitty-focus

}
