hook global ModuleLoaded x11 %{
    require-module x11-repl
}

provide-module x11-repl %{

declare-option -docstring "window id of the REPL window" str x11_repl_id

define-command -docstring %{
    x11-repl [<arguments>]: create a new window for repl interaction
    All optional parameters are forwarded to the new window
} \
    -params .. \
    x11-repl %{ x11-terminal sh -c %{
        winid="${WINDOWID:-$(xdotool search --pid ${PPID} | tail -1)}"
        printf "evaluate-commands -try-client $1 \
            'set-option current x11_repl_id ${winid}'" | kak -p "$2"
        shift 2;
        [ "$1" ] && "$@" || "$SHELL"
    } -- %val{client} %val{session} %arg{@}
}
complete-command x11-repl shell

define-command x11-send-text -params 0..1 -docstring %{
        x11-send-text [text]: Send text to the REPL window.
        If no text is passed, then the selection is used
        } %{
    evaluate-commands %sh{
        ([ "$#" -gt 0 ] && printf "%s" "$1" || printf "%s" "${kak_selection}" ) | xsel -i ||
        echo 'fail x11-send-text: failed to run xsel, see *debug* buffer for details' &&
        kak_winid=$(xdotool getactivewindow) &&
        xdotool windowactivate "${kak_opt_x11_repl_id}" key --clearmodifiers Shift+Insert &&
        xdotool windowactivate "${kak_winid}" ||
        echo 'fail x11-send-text: failed to run xdotool, see *debug* buffer for details'
    }
}

alias global repl-new x11-repl
alias global repl-send-text x11-send-text

}
