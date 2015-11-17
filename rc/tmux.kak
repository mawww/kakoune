# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## The default behaviour for the `new` command is to open an horizontal pane in a tmux session
hook global KakBegin .* %{
    %sh{
        if [ -n "$TMUX" ]; then
            echo "alias global focus tmux-focus"
            echo "alias global new tmux-new-horizontal"
        fi
    }
}

## Temporarily override the default client creation command
def -hidden -shell-params tmux-new-impl %{
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

def tmux-new-vertical -shell-params -command-completion -docstring "Create a new vertical pane in tmux" %{
    %sh{ echo "tmux-new-impl 'split-window -v' $@" }
}

def tmux-new-horizontal -shell-params -command-completion -docstring "Create a new horizontal pane in tmux" %{
    %sh{ echo "tmux-new-impl 'split-window -h' $@" }
}

def tmux-new-window -shell-params -command-completion -docstring "Create a new horizontal pane in tmux" %{
    %sh{ echo "tmux-new-impl 'new-window' $@" }
}

def -docstring "focus given client" \
    -shell-params -client-completion \
    tmux-focus %{ %sh{
    if [ $# -gt 1 ]; then
        echo "echo -color Error 'too many arguments, use focus [client]'"
    elif [ $# -eq 1 ]; then
        echo "eval -client '$1' focus"
    elif [ -n "${kak_client_env_TMUX}" ]; then
        TMUX="${kak_client_env_TMUX}" tmux select-pane -t "${kak_client_env_TMUX_PANE}" > /dev/null
    fi
} }
