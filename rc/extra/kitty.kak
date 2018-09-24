declare-option -docstring %{window type that kitty creates on new and repl calls (kitty|os)} str kitty_window_type kitty

hook -group kitty-hooks global KakBegin .* %sh{
    if [ "$TERM" = "xterm-kitty" ] && [ -z "$TMUX" ]; then
        echo "
            alias global new kitty-new
            alias global new-tab kitty-new-tab
            alias global focus kitty-focus
            alias global repl kitty-repl
            alias global send-text kitty-send-text
        "
    fi
}

define-command -docstring %{kitty-new [<command>]: create a new kak client for the current session
Optional arguments are passed as arguments to the new client} \
    -params .. \
    -command-completion \
    kitty-new %{ nop %sh{
        kitty @ new-window --no-response --window-type $kak_opt_kitty_window_type "$(command -v kak 2>/dev/null)" -c "${kak_session}" -e "$*"
}}

define-command -docstring %{kitty-new-tab [<arguments>]: create a new tab
All optional arguments are forwarded to the new kak client} \
    -params .. \
    -command-completion \
    kitty-new-tab %{ nop %sh{
        kitty @ new-window --no-response --new-tab "$(command -v kak 2>/dev/null)" -c "${kak_session}" -e "$*"
}}

define-command -params ..1 -client-completion \
    -docstring %{kitty-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    kitty-focus %{ evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "evaluate-commands -client '$1' focus"
        else
            kitty @ focus-tab --no-response -m=id:$kak_client_env_KITTY_WINDOW_ID
            kitty @ focus-window --no-response -m=id:$kak_client_env_KITTY_WINDOW_ID
        fi
}}

define-command -docstring %{kitty-repl [<arguments>]: create a new window for repl interaction
All optional parameters are forwarded to the new window} \
    -params .. \
    -shell-candidates %{
        find $(echo $PATH | tr ':' ' ') -mindepth 1 -maxdepth 1 -executable -printf "%f\n"
    } \
    kitty-repl %{ evaluate-commands %sh{
        if [ $# -eq 0 ]; then cmd="${SHELL:-/bin/sh}"; else cmd="$*"; fi
        kitty @ new-window --no-response --window-type $kak_opt_kitty_window_type --title kak_repl_window $cmd < /dev/null > /dev/null 2>&1 &
}}

define-command kitty-send-text -docstring "send the selected text to the repl window" %{
    nop %sh{
        kitty @ send-text -m=title:kak_repl_window "${kak_selection}"
    }
}
