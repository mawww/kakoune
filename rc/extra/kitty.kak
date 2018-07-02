hook global KakBegin .* %sh{
    if [ "$TERM" = "xterm-kitty" ] && [ -z "$TMUX" ]; then
        echo "
            alias global new kitty-new
            alias global focus kitty-focus
        "
    fi
}

define-command -params .. kitty-new %{
    nop %sh{ kitty @ new-window "$(command -v kak 2>/dev/null)" -c "${kak_session}" -e "$*" }
}

define-command -params ..1 -client-completion \
    -docstring %{kitty-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    kitty-focus %{ evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "evaluate-commands -client '$1' focus"
        else
            kitty @ focus-window -m=id:$kak_client_env_KITTY_WINDOW_ID
        fi
    }
}
