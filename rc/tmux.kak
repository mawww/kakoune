# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## The default behaviour for the `new` command is to open an horizontal pane in a tmux session
hook global KakBegin .* %{
    %sh{
        if [ -n "$TMUX" ]; then
            echo "alias global focus tmux-focus"
            echo "alias global new tmux-new-horizontal"
            echo "alias global repl tmux-repl-horizontal"
            echo "alias global send-text tmux-send-text"
        fi
    }
}

## Temporarily override the default client creation command
def -hidden -params 1.. tmux-new-impl %{
    %sh{
        if [ -z "$TMUX" ]; then
            echo "echo -color Error This command is only available in a tmux session"
            exit
        fi
        tmux_args="$1"
        shift
        if [ $# -ne 0 ]; then kakoune_params="-e '$@'"; fi
        tmux $tmux_args "kak -c ${kak_session} ${kakoune_params}" < /dev/null > /dev/null 2>&1 &
    }
}

def tmux-new-vertical -params .. -command-completion -docstring "Create a new vertical pane in tmux" %{
    tmux-new-impl 'split-window -v' %arg{@}
}

def tmux-new-horizontal -params .. -command-completion -docstring "Create a new horizontal pane in tmux" %{
    tmux-new-impl 'split-window -h' %arg{@}
}

def tmux-new-window -params .. -command-completion -docstring "Create a new window in tmux" %{
    tmux-new-impl 'new-window' %arg{@}
}

def -hidden -params 1..2 tmux-repl-impl %{
    %sh{
        if [ -z "$TMUX" ]; then
            echo "echo -color Error This command is only available in a tmux session"
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

def tmux-repl-vertical -params 0..1 -command-completion -docstring "Create a new vertical pane in tmux for repl interaction" %{
    tmux-repl-impl 'split-window -v' %arg{@}
}

def tmux-repl-horizontal -params 0..1 -command-completion -docstring "Create a new horizontal pane in tmux for repl interaction" %{
    tmux-repl-impl 'split-window -h' %arg{@}
}

def tmux-repl-window -params 0..1 -command-completion -docstring "Create a new window in tmux for repl interaction" %{
    tmux-repl-impl 'new-window' %arg{@}
}

def tmux-send-text -docstring "Send selected text to the repl pane in tmux" %{
    %sh{
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

def -docstring "focus given client" \
    -params 0..1 -client-completion \
    tmux-focus %{ %sh{
    if [ $# -gt 1 ]; then
        echo "echo -color Error 'too many arguments, use focus [client]'"
    elif [ $# -eq 1 ]; then
        echo "eval -client '$1' focus"
    elif [ -n "${kak_client_env_TMUX}" ]; then
        TMUX="${kak_client_env_TMUX}" tmux select-pane -t "${kak_client_env_TMUX_PANE}" > /dev/null
    fi
} }
