provide-module sway %{

# Ensure we're actually in Sway
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || 
    [ -n "$SWAYSOCK" ] ||
    echo 'fail SWAYSOCK is not set'
}

require-module 'wayland'

alias global sway-terminal-window wayland-terminal-window

define-command sway-terminal-vertical -params 1.. -docstring '
    sway-terminal-vertical <program> [<arguments>]: create a new terminal as a Sway window
    The current pane is split into two, top and bottom
    The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{swaymsg split vertical}
    wayland-terminal-window %arg{@}
}
complete-command sway-terminal-vertical shell

define-command sway-terminal-horizontal -params 1.. -docstring '
    sway-terminal-horizontal <program> [<arguments>]: create a new terminal as a Sway window
    The current pane is split into two, left and right
    The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{swaymsg split horizontal}
    wayland-terminal-window %arg{@}
}
complete-command sway-terminal-horizontal shell

define-command sway-terminal-tab -params 1.. -docstring '
    sway-terminal-tab <program> [<arguments>]: create a new terminal as a Sway window
    The program passed as argument will be executed in the new terminal' \
%{
    nop %sh{swaymsg 'split horizontal; layout tabbed'}
    wayland-terminal-window %arg{@}
}
complete-command sway-terminal-tab shell

define-command sway-focus-pid -hidden %{
    evaluate-commands %sh{
        pid=$kak_client_pid

        # Try to focus a window with the current PID, walking up the tree of 
        # parent processes until the focus eventually succeeds
        while ! swaymsg [pid=$pid] focus > /dev/null 2> /dev/null ; do
            # Replace the current PID with its parent PID
            pid=$(ps -p $pid -o ppid=)

            # If we couldn't get a PPID for some reason, or it's 1 or less, we 
            # should just fail. 
            if [ -z $pid ] || [ $pid -le 1 ]; then
                echo "fail Can't find PID for Sway window to focus"
                break
            fi
        done
    }
}

define-command sway-focus -params ..1 -docstring '
sway-focus [<kakoune_client>]: focus a given client''s window.
If no client is passed, then the current client is used' \
%{
    # Quick branch to make sure we're calling sway-focus-pid from the client 
    # the user wants to focus on.
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' sway-focus-pid" "$1"
        else
            echo sway-focus-pid
        fi
    }
}
complete-command -menu sway-focus client

alias global focus sway-focus

}
