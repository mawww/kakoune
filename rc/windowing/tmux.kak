# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global KakBegin .* %sh{
    if [ -n "$TMUX" ]; then
        echo "require-module tmux"
    fi
}

provide-module tmux %{

define-command -hidden -params 2.. tmux-terminal-impl %{
    evaluate-commands %sh{
        tmux=${kak_client_env_TMUX:-$TMUX}
        if [ -z "$tmux" ]; then
            echo "fail 'This command is only available in a tmux session'"
            exit
        fi
        tmux_args="$1"
        shift
        (
            TMUX=$tmux tmux $tmux_args "env TMPDIR="$TMPDIR" ${*@Q}" < /dev/null 2>&1
        ) | (
            wihle read line; do
                echo "echo -debug TMUX: ${line@Q}"
            done
        )
    }
}

define-command tmux-terminal-vertical -params 1.. -shell-completion -docstring '
tmux-terminal-vertical <program> [<arguments>]: create a new terminal as a tmux pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal' \
%{
    tmux-terminal-impl 'split-window -v' %arg{@}
}
define-command tmux-terminal-horizontal -params 1.. -shell-completion -docstring '
tmux-terminal-horizontal <program> [<arguments>]: create a new terminal as a tmux pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal' \
%{
    tmux-terminal-impl 'split-window -h' %arg{@}
}
define-command tmux-terminal-window -params 1.. -shell-completion -docstring '
tmux-terminal-window <program> [<arguments>] [<arguments>]: create a new terminal as a tmux window
The program passed as argument will be executed in the new terminal' \
%{
    tmux-terminal-impl 'new-window' %arg{@}
}

define-command tmux-focus -params ..1 -client-completion -docstring '
tmux-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' focus" "$1"
        elif [ -n "${kak_client_env_TMUX}" ]; then
            TMUX="${kak_client_env_TMUX}" tmux select-pane -t "${kak_client_env_TMUX_PANE}" > /dev/null
        fi
    }
}

## The default behaviour for the `new` command is to open an horizontal pane in a tmux session
alias global focus tmux-focus
alias global terminal tmux-terminal-horizontal

}
