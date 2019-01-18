# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

declare-option -docstring "tmux pane id in which the REPL is running" str tmux_repl_id

hook global KakBegin .* %sh{
    if [ -n "$TMUX" ]; then
        VERSION_TMUX=$(tmux -V | cut -d' ' -f2)
        VERSION_TMUX=${VERSION_TMUX%%.*}

        if [ "${VERSION_TMUX}" = "master" ] \
            || [ "${VERSION_TMUX}" -ge 2 ]; then
            echo "
                alias global repl tmux-repl-horizontal
                alias global send-text tmux-send-text
            "
        else
            echo "
                alias global repl tmux-repl-disabled
                alias global send-text tmux-repl-disabled
            "
        fi
    fi
}

define-command -hidden -params 1..2 tmux-repl-impl %{
    evaluate-commands %sh{
        if [ -z "$TMUX" ]; then
            echo "echo -markup '{Error}This command is only available in a tmux session'"
            exit
        fi
        tmux_args="$1"
        shift
        tmux_cmd="$@"
        tmux $tmux_args $tmux_cmd
        printf "set-option global tmux_repl_id '%s'" $(tmux display-message -p '#{session_id}:#{window_id}.#{pane_id}')
    }
}

define-command tmux-repl-vertical -params 0..1 -command-completion -docstring "Create a new vertical pane for repl interaction" %{
    tmux-repl-impl 'split-window -v' %arg{@}
}

define-command tmux-repl-horizontal -params 0..1 -command-completion -docstring "Create a new horizontal pane for repl interaction" %{
    tmux-repl-impl 'split-window -h' %arg{@}
}

define-command tmux-repl-window -params 0..1 -command-completion -docstring "Create a new window for repl interaction" %{
    tmux-repl-impl 'new-window' %arg{@}
}

define-command -hidden tmux-send-text -params 0..1 -docstring "tmux-send-text [text]: Send text(append new line) to the REPL pane.
  If no text is passed, then the selection is used" %{
    nop %sh{
        if [ $# -eq 0 ]; then 
            tmux set-buffer -b kak_selection "${kak_selection}"
        else
            tmux set-buffer -b kak_selection "$1"
        fi
        tmux paste-buffer -b kak_selection -t "$kak_opt_tmux_repl_id"
    }
}

define-command -hidden tmux-repl-disabled %{ evaluate-commands %sh{
    VERSION_TMUX=$(tmux -V)
    printf %s "echo -markup %{{Error}The version of tmux is too old: got ${VERSION_TMUX}, expected >= 2.x}"
} }
