# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## The default behaviour for the `new` command is to open an horizontal pane in a tmux session
hook global KakBegin .* %sh{
    if [ -n "$TMUX" ]; then
        echo "
            alias global focus tmux-focus
            alias global new tmux-new-horizontal
            alias global terminal tmux-terminal-horizontal
        "
    fi
}

define-command -hidden -params 2.. tmux-terminal-impl %{
    evaluate-commands %sh{
        tmux=${kak_client_env_TMUX:-$TMUX}
        if [ -z "$tmux" ]; then
            echo "fail 'This command is only available in a tmux session'"
            exit
        fi
        tmux_args="$1"
        shift
        TMUX=$tmux tmux $tmux_args env TMPDIR="$TMPDIR" sh -c "$*" < /dev/null > /dev/null 2>&1 &
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
tmux-terminal-window <program> [<arguments>]: create a new terminal as a tmux window
The program passed as argument will be executed in the new terminal' \
%{
    tmux-terminal-impl 'new-window' %arg{@}
}

define-command tmux-new-vertical -params .. -command-completion -docstring '
tmux-new-vertical [<commands>]: create a new kakoune client as a tmux pane
The current pane is split into two, top and bottom
The optional arguments are passed as commands to the new client' \
%{
    tmux-terminal-vertical "kak -c %val{session} -e '%arg{@}'"
}
define-command tmux-new-horizontal -params .. -command-completion -docstring '
tmux-new-horizontal [<commands>]: create a new kakoune client as a tmux pane
The current pane is split into two, left and right
The optional arguments are passed as commands to the new client' \
%{
    tmux-terminal-horizontal "kak -c %val{session} -e '%arg{@}'"
}
define-command tmux-new-window -params .. -command-completion -docstring '
tmux-new-window [<commands>]: create a new kakoune client as a tmux window
The optional arguments are passed as commands to the new client' \
%{
    tmux-terminal-window "kak -c %val{session} -e '%arg{@}'"
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
