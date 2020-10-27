hook global ModuleLoaded x11 %{
    require-module x11-repl
}

provide-module x11-repl %{

declare-option -docstring "window id of the REPL window" str x11_repl_id

# termcmd should already be set in x11.kak
define-command -docstring %{
    x11-repl [<arguments>]: create a new window for repl interaction
    All optional parameters are forwarded to the new window
} \
    -params .. \
    -shell-completion \
    x11-repl %{ evaluate-commands %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo 'fail termcmd option is not set'
           exit
        fi
        if [ $# -eq 0 ]; then cmd="${SHELL:-sh}"; else cmd="$@"; fi
        # put the window id of the REPL into the option x11_repl_id
        wincmd=$(printf 'printf "evaluate-commands -try-client %s \
                    set-option current x11_repl_id ${WINDOWID}" | kak -p %s' \
                    "${kak_client}" "${kak_session}")
        setsid ${kak_opt_termcmd} "${SHELL} -c '${wincmd} && ${cmd}'" \
            < /dev/null > /dev/null 2>&1 &
}}

define-command x11-send-text -docstring "send the selected text to the repl window" %{
    evaluate-commands %sh{
        printf %s\\n "${kak_selection}" | xsel -i ||
        echo 'fail x11-send-text: failed to run xsel, see *debug* buffer for details' &&
        xdotool windowactivate "${kak_opt_x11_repl_id}" key --clearmodifiers Shift+Insert ||
        echo 'fail x11-send-text: failed to run xdotool, see *debug* buffer for details'
    }
}

alias global repl-new x11-repl
alias global repl-send-text x11-send-text

}
