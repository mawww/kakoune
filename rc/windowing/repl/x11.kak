hook global ModuleLoaded x11 %{
    require-module x11-repl
}

provide-module x11-repl %{

# termcmd should already be set in x11.kak
define-command -docstring %{x11-repl [<arguments>]: create a new window for repl interaction
All optional parameters are forwarded to the new window} \
    -params .. \
    -shell-completion \
    x11-repl %{ evaluate-commands %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo "echo -markup '{Error}termcmd option is not set'"
           exit
        fi
        if [ $# -eq 0 ]; then cmd="${SHELL:-sh}"; else cmd="$@"; fi
        # The escape sequence in the printf command sets the terminal's title:
        setsid ${kak_opt_termcmd} "printf '\e]2;kak_repl_window\a' \
                && ${cmd}" < /dev/null > /dev/null 2>&1 &
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

}
