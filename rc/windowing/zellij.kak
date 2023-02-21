# Zellij - terminal multiplexer
# https://zellij.dev
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module zellij %{

evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || \
    [ -n "$ZELLIJ" -a -n "$ZELLIJ_SESSION_NAME" ] || \
    echo "fail zellij not detected"
}

evaluate-commands %sh{
    which zellij &> /dev/null || \
    echo "fail zellij not detected"
}

define-command -hidden zellij-probe %{
    evaluate-commands %sh{
        [ -n "$kak_client_env_ZELLIJ_SESSION_NAME" ] || \
        echo "fail 'This command is only available from within a zellij session'"
    }
}

define-command -hidden -params 2.. zellij-terminal-impl %{
    zellij-probe
    nop %sh{
        zellij_session="$ZELLIJ_SESSION_NAME"
        zellij_args="$1"
        shift
        # Terminals spawned through `connect.kak` yield panes with
        # unhinged names (sh -c\nkak_opt_prelude...) that `zellij`
        # struggles to escape & render correctly.
        # Overriding it here with `--name ""`
        zellij \
            --session "$zellij_session" \
            run \
            --name "" \
            --close-on-exit \
            $zellij_args \
            -- \
            "$@"
    }
}

define-command zellij-terminal-vertical -params 1.. -docstring '
zellij-terminal-vertical <program> [<arguments>]: create a new terminal as a zellij pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal' \
%{
    zellij-terminal-impl '--direction down' %arg{@}
}
complete-command zellij-terminal-vertical shell

define-command zellij-terminal-horizontal -params 1.. -docstring '
zellij-terminal-horizontal <program> [<arguments>]: create a new terminal as a zellij pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal' \
%{
    zellij-terminal-impl '--direction right' %arg{@}
}
complete-command zellij-terminal-horizontal shell

define-command zellij-terminal-popup -params 1.. -docstring '
zellij-terminal-popup <program> [<arguments>]: create a new terminal as a zellij floating pane
The program passed as argument will be executed in the new terminal' \
%{
    zellij-terminal-impl '--floating' %arg{@}
}
complete-command zellij-terminal-popup shell

define-command zellij-focus -params ..1 -docstring '
zellij-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    zellij-probe
    # I don't know if there is any way to do it currently.
    # In most cases though this should not be necessary as focus
    # should automatically go back to the kakoune's pane after the
    # spawned pane dies
    echo -debug "focus not supported"
}
complete-command -menu zellij-focus client

define-command zellij-action -params 1.. -docstring '
zellij-action <action> [<parameters>]: send an action to a zellij session
List of all available actions: `zellij action --help`' \
%{
    zellij-probe
    nop %sh{
        zellij_session="$ZELLIJ_SESSION_NAME"
        zellij \
            --session "$zellij_session" \
            action \
            "$@"
    }
}

## The default behaviour for the `new` command is to open an horizontal pane in a zellij session
alias global focus zellij-focus
alias global terminal zellij-terminal-horizontal
alias global popup zellij-terminal-popup

}
