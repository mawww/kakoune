hook global ModuleLoaded kitty %{
    require-module kitty-repl
}

provide-module kitty-repl %{

define-command -params .. \
    -docstring %{
        kitty-repl [<arguments>]: Create a new window for repl interaction.

        All optional parameters are forwarded to the new window.
    } \
    kitty-repl %{
    nop %sh{
        if [ $# -eq 0 ]; then
            cmd="${SHELL:-/bin/sh}"
        else
            cmd="$*"
        fi

       match=""
        if [ -n "$kak_client_env_KITTY_WINDOW_ID" ]; then
            match="--match=id:$kak_client_env_KITTY_WINDOW_ID"
        fi

        listen=""
        if [ -n "$kak_client_env_KITTY_LISTEN_ON" ]; then
            listen="--to=$kak_client_env_KITTY_LISTEN_ON"
        fi

        kitty @ $listen launch --no-response --keep-focus --type="$kak_opt_kitty_window_type" --title=kak_repl_window --cwd="$PWD" $match $cmd
    }
}
complete-command kitty-repl shell

define-command -hidden -params 0..1 \
    -docstring %{
        kitty-send-text [text]: Send text to the REPL window.

        If no text is passed, the selection is used.
    } \
    kitty-send-text %{
    nop %sh{
        if [ $# -eq 0 ]; then
            text="$kak_selection"
        else
            text="$1"
        fi
        kitty @ send-text --match=title:kak_repl_window "$text"
    }
}

alias global repl-new kitty-repl
alias global repl-send-text kitty-send-text

}
