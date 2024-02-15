# https://wezfurlong.org/wezterm/index.html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾


provide-module wezterm %{

# ensure that we're running under screen
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "$WEZTERM_UNIX_SOCKET" ] || echo 'fail wezterm not detected'
}

define-command wezterm-terminal-impl -hidden -params 2.. %{
    nop %sh{
        wezterm cli "$@"
    }
}

define-command wezterm-terminal-vertical -params 1.. -docstring '
wezterm-terminal-vertical <program> [<arguments>]: create a new terminal as a wezterm pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal' \
%{
    wezterm-terminal-impl split-pane --cwd "%val{client_env_PWD}" --bottom --pane-id "%val{client_env_WEZTERM_PANE}" -- %arg{@}
}
complete-command wezterm-terminal-vertical shell

define-command wezterm-terminal-horizontal -params 1.. -docstring '
wezterm-terminal-horizontal <program> [<arguments>]: create a new terminal as a wezterm pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal' \
%{
    wezterm-terminal-impl split-pane --cwd "%val{client_env_PWD}" --right --pane-id "%val{client_env_WEZTERM_PANE}" -- %arg{@}
}
complete-command wezterm-terminal-horizontal shell

define-command wezterm-terminal-tab -params 1.. -docstring '
wezterm-terminal-tab <program> [<arguments>]: create a new terminal as a wezterm tab
The program passed as argument will be executed in the new terminal' \
%{
    wezterm-terminal-impl spawn --cwd "%val{client_env_PWD}" --pane-id "%val{client_env_WEZTERM_PANE}" -- %arg{@}
}
complete-command wezterm-terminal-tab shell

define-command wezterm-terminal-window -params 1.. -docstring '
wezterm-terminal-window <program> [<arguments>]: create a new terminal as a wezterm window
The program passed as argument will be executed in the new terminal' \
%{
    wezterm-terminal-impl spawn --cwd "%val{client_env_PWD}" --new-window --pane-id "%val{client_env_WEZTERM_PANE}" -- %arg{@}
}
complete-command wezterm-terminal-window shell

define-command wezterm-focus -params ..1 -docstring '
wezterm-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "
                evaluate-commands -client '$1' focus
                "
        elif [ -n "${kak_client_env_WEZTERM_PANE}" ]; then
            wezterm cli activate-pane --pane-id "${kak_client_env_WEZTERM_PANE}" > /dev/null 2>&1
        fi
    }
}
complete-command -menu wezterm-focus client

alias global focus wezterm-focus

}
