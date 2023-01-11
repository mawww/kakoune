provide-module dtach-repl %{

# test if dtach is installed
evaluate-commands %sh{
    [ -n "$(command -v dtach)" ] || echo 'fail dtach not found'
}

declare-option -docstring "id of the REPL" str dtach_repl_id

define-command -docstring %{
    dtach-repl [<arguments>]: create a new terminal window for repl interaction
    All optional parameters are forwarded to the new terminal window
} \
    -params .. \
    dtach-repl %{ terminal sh -c %{
        file="$(mktemp -u -t kak_dtach_repl.XXXXX)"
        trap 'rm -f "${file}"' EXIT
        printf "evaluate-commands -try-client $1 \
            'set-option current dtach_repl_id ${file}'" | kak -p "$2"
        shift 2
        dtach -c "${file}" -E sh -c "${@:-$SHELL}" || "${@:-$SHELL}"
    } -- %val{client} %val{session} %arg{@}
}
complete-command dtach-repl shell

define-command dtach-send-text -params 0..1 -docstring %{
        dtach-send-text [text]: Send text to the REPL.
        If no text is passed, then the selection is used
        } %{
    nop %sh{
        printf "%s" "${@:-$kak_selection}" | dtach -p "$kak_opt_dtach_repl_id"
    }
}

alias global repl-new dtach-repl
alias global repl-send-text dtach-send-text

}
