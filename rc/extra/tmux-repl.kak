# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global KakBegin .* %{
    %sh{
        if [ -n "$TMUX" ]; then
            VERSION_TMUX=$(tmux -V)
            VERSION_TMUX=$(expr "${VERSION_TMUX}" : 'tmux \([0-9]*\).*')

            if [ "${VERSION_TMUX}" -gt 1 ]; then
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
}

define-command -hidden -params 1..2 tmux-repl-impl %{
    %sh{
        if [ -z "$TMUX" ]; then
            echo "echo -markup '{Error}This command is only available in a tmux session'"
            exit
        fi
        tmux_args="$1"
        shift
        tmux_cmd="$@"
        tmux $tmux_args $tmux_cmd
        tmux set-buffer -b kak_repl_window $(tmux display-message -p '#I')
        tmux set-buffer -b kak_repl_pane $(tmux display-message -p '#P')
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

define-command -hidden tmux-send-text -docstring "Send the selected text to the repl pane" %{
    nop %sh{
        tmux set-buffer -b kak_selection "${kak_selection}"
        kak_orig_window=$(tmux display-message -p '#I')
        kak_orig_pane=$(tmux display-message -p '#P')
        tmux select-window -t:$(tmux show-buffer -b kak_repl_window)
        tmux select-pane -t:.$(tmux show-buffer -b kak_repl_pane)
        tmux paste-buffer -b kak_selection
        tmux select-window -t:${kak_orig_window}
        tmux select-pane -t:.${kak_orig_pane}
    }
}

define-command -hidden tmux-repl-disabled %{ %sh{
    VERSION_TMUX=$(tmux -V)
    printf %s "echo -markup %{{Error}The version of tmux is too old: got ${VERSION_TMUX}, expected >= 2.x}"
} }
