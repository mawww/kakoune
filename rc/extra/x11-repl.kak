%sh{
    if ! command -v xdotool >/dev/null; then
        echo 'echo -debug x11-repl: warning, command dependency unmet: xdotool'
    fi
}

# termcmd should already be set-option in x11.kak
define-command -docstring %{x11-repl [<arguments>]: create a new window for repl interaction
All optional parameters are forwarded to the new window} \
    -params .. \
    -command-completion \
    x11-repl %{ %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo "echo -markup '{Error}termcmd option is not set-option'"
           exit
        fi
        if [ $# -eq 0 ]; then cmd="${SHELL:-sh}"; else cmd="$@"; fi
        setsid ${kak_opt_termcmd} ${cmd} -t kak_repl_window < /dev/null > /dev/null 2>&1 &
}}

define-command x11-send-text -docstring "send the selected text to the repl window" %{
    nop %sh{
        printf %s\\n "${kak_selection}" | xsel -i
        wid=$(xdotool getactivewindow)
        xdotool search --name kak_repl_window windowactivate
        xdotool key --clearmodifiers "Shift+Insert"
        xdotool windowactivate $wid
    }
}

alias global repl x11-repl
alias global send-text x11-send-text
