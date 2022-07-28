provide-module sway %{

# Ensure we're actually in Sway
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || 
    [ -n "$SWAYSOCK" ] ||
    echo 'fail SWAYSOCK is not set'
}

require-module 'wayland'

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

unalias global focus
alias global focus sway-focus

}
