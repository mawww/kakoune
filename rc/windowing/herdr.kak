# https://herdr.dev/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# Requires `jq` to parse protocol messages, and `netcat` to focus a pane.

provide-module herdr %{

evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "${HERDR_SOCKET_PATH}" ] || echo 'fail herdr not detected'
}

define-command -hidden -params 2.. herdr-terminal-impl %{
    evaluate-commands %sh{
        SOCKET_PATH=${kak_client_env_HERDR_SOCKET_PATH:-$HERDR_SOCKET_PATH}
        if [ -z "${SOCKET_PATH}" ]; then
            echo "fail 'This command is only available in a herdr session'"
            exit
        fi

        terminal_type="$1"
        shift
        case "${terminal_type}" in
            pane)
                pane_direction="$1"
                shift
                pane_info=$(herdr pane split --focus --direction "${pane_direction}")
                new_pane_id=$(printf '%s\n' "${pane_info}" | jq -r '.result.pane.pane_id')
                herdr pane run "${new_pane_id}" "{ $*; }; exit"
            ;;

            workspace)
                shift
                workspace_info=$(herdr workspace create --focus)
                root_pane_id=$(printf '%s\n' "${workspace_info}" | jq -r '.result.root_pane.pane_id')
                herdr pane run "${root_pane_id}" "{ $*; }; exit"
            ;;

            *)
                printf 'Unsupported terminal type: %s\n' "${terminal_type}" >&2
                exit 1
            ;;
        esac
    }
}

define-command herdr-terminal-vertical -params 1.. -docstring %{
    herdr-terminal-vertical <program> [<arguments>]: create a new terminal as a herdr pane
    The current pane is split into two, top and bottom
    The program passed as argument will be executed in the new terminal
} %{
    herdr-terminal-impl 'pane' 'down' %arg{@}
}
complete-command herdr-terminal-vertical shell

define-command herdr-terminal-horizontal -params 1.. -docstring %{
    herdr-terminal-horizontal <program> [<arguments>]: create a new terminal as a herdr pane
    The current pane is split into two, left and right
    The program passed as argument will be executed in the new terminal
} %{
    herdr-terminal-impl 'pane' 'right' %arg{@}
}
complete-command herdr-terminal-horizontal shell

define-command herdr-terminal-workspace -params 1.. -docstring %{
    herdr-terminal-workspace <program> [<arguments>]: create a new terminal as a herdr workspace
    The program passed as argument will be executed in the new terminal
} %{
    herdr-terminal-impl 'workspace' 'any' %arg{@}
}
complete-command herdr-terminal-workspace shell

define-command -hidden herdr-focus-me %{ nop %sh{
    herdr workspace focus "${kak_client_env_HERDR_WORKSPACE_ID}"
    herdr tab focus "${kak_client_env_HERDR_TAB_ID}"
    cat <<-EOF | netcat -U "${kak_client_env_HERDR_SOCKET_PATH}"
	{ "id": "1", "method": "pane.focus", "params": { "pane_id": "%s" } }
	EOF
} }
define-command herdr-focus -params ..1 -docstring %{
    herdr-focus [<client>]: focus the given client, or the current one otherwise
} %{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf 'evaluate-commands -client "%s" herdr-focus-me\n' "$1"
        else
            echo herdr-focus-me
        fi
    }
}
complete-command -menu herdr-focus client

alias global focus herdr-focus

}
