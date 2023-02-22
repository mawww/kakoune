# https://zellij.dev/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module zellij %{

# ensure we're running under zellij
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "$ZELLIJ" ] || echo 'fail zellij not detected'
}

declare-option -hidden -docstring %{zellij run options (please check out "zellij run --help" for detail)}\
    str zellij_run_options ""

define-command -hidden -params .. zellij-run %{
    nop %sh{
        zellij run $kak_opt_zellij_run_options -- "$@"
    }
    set global zellij_run_options ""
    unset buffer zellij_run_options
    unset window zellij_run_options
}

define-command -hidden -params 1.. zellij-terminal-impl %{
    set global zellij_run_options "--close-on-exit"
    zellij-run %arg{@}
}
complete-command zellij-terminal-impl shell

define-command zellij-terminal-vertical -params 1.. -docstring '
zellij-terminal-vertical <program> [<arguments>]: create a new terminal as a zellij pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal' \
%{
    set global zellij_run_options "--direction down"
    zellij-run %arg{@}
}
complete-command zellij-terminal-vertical shell

define-command zellij-terminal-horizontal -params 1.. -docstring '
zellij-terminal-horizontal <program> [<arguments>]: create a new terminal as a zellij pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal' \
%{
    set global zellij_run_options "--direction right"
    zellij-run %arg{@}
}
complete-command zellij-terminal-horizontal shell

define-command zellij-focus -params ..1 -docstring '
zellij-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{ evaluate-commands %sh{
    if [ $# -eq 1 ]; then
        printf "evaluate-commands -client '%s' focus" "$1"
    elif [ -n "${kak_client_env_ZELLIJ}" ]; then
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-zellij.XXXXXXXX)/dump-screen
        pane_count=0
        while [ $((pane_count+=1)) -lt 10 ]; do
             if zellij action dump-screen "$output" && grep "${kak_client}@\[$kak_session\]" "$output" > /dev/null ; then
                 break;
             fi
             zellij action focus-next-pane
        done
        rm -r $(dirname $output)
    fi
}}
complete-command -menu zellij-focus client

## The default behaviour for the `new` command is to open an horizontal pane in a zellij session
alias global focus zellij-focus
alias global terminal zellij-terminal-impl

}

