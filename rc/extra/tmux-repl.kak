# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

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
        tmux set-buffer -b kak_repl_window $(tmux display-message -p '#I')
        tmux set-buffer -b kak_repl_pane $(tmux display-message -p '#{pane_id}')
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
        kak_orig_window=$(tmux display-message -p '#I')
        kak_orig_pane=$(tmux display-message -p '#P')
        tmux select-window -t:$(tmux show-buffer -b kak_repl_window)
        tmux select-pane -t:.$(tmux show-buffer -b kak_repl_pane)
        tmux paste-buffer -b kak_selection
        tmux select-window -t:${kak_orig_window}
        tmux select-pane -t:.${kak_orig_pane}
    }
}

define-command -hidden tmux-repl-disabled %{ evaluate-commands %sh{
    VERSION_TMUX=$(tmux -V)
    printf %s "echo -markup %{{Error}The version of tmux is too old: got ${VERSION_TMUX}, expected >= 2.x}"
} }
