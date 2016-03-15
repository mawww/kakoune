# termcmd should already be set in x11.kak
def -docstring 'create a new window for repl interaction' \
    -params 0..1 \
    -command-completion \
    x11-repl %{ %sh{
        if [ -z "${kak_opt_termcmd}" ]; then
           printf %s "echo -color Error 'termcmd option is not set'"
           exit
        fi
        if [ $# -eq 0 ]; then cmd="bash"; else cmd="$1"; fi
        setsid ${kak_opt_termcmd} ${cmd} -t kak_repl_window < /dev/null > /dev/null 2>&1 &
}}

def x11-send-text -docstring "send selected text to the repl window" %{
    nop %sh{
        printf %s "${kak_selection}" | xsel -i
        wid=$(xdotool getactivewindow)
        xdotool search --name kak_repl_window windowactivate
        xdotool key --clearmodifiers "Shift+Insert"
        xdotool windowactivate $wid
    }
}

alias global repl x11-repl
alias global send-text x11-send-text
