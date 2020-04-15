hook global ModuleLoaded kitty %{
    require-module kitty-repl
}

provide-module kitty-repl %{

define-command -params .. -shell-completion \
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
        kitty @ new-window --no-response --window-type $kak_opt_kitty_window_type --title kak_repl_window --cwd "$PWD" $cmd < /dev/null > /dev/null 2>&1 &
    }
}

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
        kitty @ send-text -m=title:kak_repl_window "$text"
    }
}

alias global repl kitty-repl
alias global send-text kitty-send-text

}
