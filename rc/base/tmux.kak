# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## The default behaviour for the `new` command is to open an horizontal pane in a tmux session
hook global KakBegin .* %sh{
    if [ -n "$TMUX" ]; then
        echo "
            alias global focus tmux-focus
            alias global new tmux-new-horizontal
        "
    fi
}

## Temporarily override the default client creation command
define-command -hidden -params 1.. tmux-new-impl %{
    evaluate-commands %sh{
        tmux=${kak_client_env_TMUX:-$TMUX}
        if [ -z "$tmux" ]; then
            echo "echo -markup '{Error}This command is only available in a tmux session'"
            exit
        fi
        tmux_args="$1"
        shift
        if [ $# -ne 0 ]; then kakoune_params="-e '$@'"; fi
        TMUX=$tmux tmux $tmux_args "env TMPDIR='${TMPDIR}' kak -c ${kak_session} ${kakoune_params}" < /dev/null > /dev/null 2>&1 &
    }
}

define-command tmux-new-vertical -params .. -command-completion -docstring "Split the current pane into two, top and bottom" %{
    tmux-new-impl 'split-window -v' %arg{@}
}

define-command tmux-new-horizontal -params .. -command-completion -docstring "Split the current pane into two, left and right" %{
    tmux-new-impl 'split-window -h' %arg{@}
}

define-command tmux-new-window -params .. -command-completion -docstring "Create a new window" %{
    tmux-new-impl 'new-window' %arg{@}
}

define-command -docstring %{tmux-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    -params ..1 -client-completion \
    tmux-focus %{ evaluate-commands %sh{
    if [ $# -eq 1 ]; then
        printf %s\\n "evaluate-commands -client '$1' focus"
    elif [ -n "${kak_client_env_TMUX}" ]; then
        TMUX="${kak_client_env_TMUX}" tmux select-pane -t "${kak_client_env_TMUX_PANE}" > /dev/null
    fi
} }
