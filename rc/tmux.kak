# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## Can be one of "horizontal", "vertical" or "window"
## When a new client is created, an horizontal pane, a vertical pane or a window will respectively be made
decl str tmux_default_direction "horizontal"

## The default behaviour for the `new` command is to open an horizontal pane in a tmux session
hook global KakBegin .* %{
    %sh{
        if [ -n "$TMUX" ]; then
            tmuxcmd=""

            case "${kak_opt_tmux_default_direction}" in
                vertical) tmuxcmd="tmux split-window -v";;
                window) tmuxcmd="tmux new-window";;
                horizontal) tmuxcmd="tmux split-window -h";;
                *) echo "invalid value: ${kak_opt_tmux_default_direction}" >&2;;
            esac

            if [ -n "${tmuxcmd}" ]; then
                echo "set global termcmd '${tmuxcmd}'"
            fi

            echo "alias global focus focus-tmux"
        fi
    }
}

## Temporarily override the default client creation command
def -hidden -shell-params tmux_override_termcmd %{
    %sh{
        if [ -z "$TMUX" ]; then
            echo "echo -color Error This command is only available in a tmux session"
        else
            readonly cmd_override="$1"
            readonly termcmd="${kak_opt_termcmd}"

            shift
            echo "
                set current termcmd '${cmd_override}'
                eval new $@
                set current termcmd '${termcmd//'/\\'}'
            "
        fi
    }
}

def tmux-new-vertical -shell-params -command-completion -docstring "Create a new vertical pane in tmux" %{
    %sh{
        echo "eval %{tmux_override_termcmd 'tmux split-window -v' $@}"
    }
}

def tmux-new-horizontal -shell-params -command-completion -docstring "Create a new horizontal pane in tmux" %{
    %sh{
        echo "eval %{tmux_override_termcmd 'tmux split-window -h' $@}"
    }
}

def -docstring "focus given client" \
    -shell-params -client-completion \
    focus-tmux %{ %sh{
    if [ $# -gt 1 ]; then
        echo "echo -color Error 'too many arguments, use focus [client]'"
    elif [ $# -eq 1 ]; then
        echo "eval -client '$1' focus"
    elif [ -n "${kak_client_env_TMUX}" ]; then
        TMUX="${kak_client_env_TMUX}" tmux select-pane -t "${kak_client_env_TMUX_PANE}" > /dev/null
    fi
} }
