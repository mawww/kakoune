# termcmd should already be set in x11.kak
def -docstring %{x11-repl [<arguments>]: create a new window for repl interaction
All optional parameters are forwarded to the new window} \
    -params .. \
    -command-completion \
    x11-repl %{ %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           echo "echo -color Error 'termcmd option is not set'"
           exit
        fi
        if [ $# -eq 0 ]; then cmd="bash"; else cmd="$@"; fi
        setsid ${kak_opt_termcmd} ${cmd} -t kak_repl_window < /dev/null > /dev/null 2>&1 &
}}

def x11-send-text -docstring "send the selected text to the repl window" %{
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
