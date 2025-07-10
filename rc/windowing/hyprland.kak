# https://hypr.land
provide-module hyprland %{

# Ensure we're actually in Hyprland
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] ||
    [ "$XDG_CURRENT_DESKTOP"  = "Hyprland" ] &&
    [ -n "$HYPRLAND_INSTANCE_SIGNATURE" ] ||
    echo 'fail hyprland not detected'
}

require-module 'wayland'

alias global hyprland-terminal-window wayland-terminal-window

define-command hyprland-focus-pid -hidden %{
    evaluate-commands %sh{
        pid=$kak_client_pid
        while [ "$(hyprctl dispatch focuswindow pid:$pid)" != "ok" ]; do
            pid=$(ps -p $pid -o ppid= | tr -d " ")
            if [ -z $pid ] || [ $pid -le 1 ]; then
                echo "fail Can't find PID for Sway window to focus"
                break
            fi
        done
    }
}

define-command hyprland-focus -params ..1 -docstring '
hyprland-focus [<kakoune_client>]: focus a given client''s window.
If no client is passed, then the current client is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' hyprland-focus-pid" "$1"
        else
            echo hyprland-focus-pid
        fi
    }
}
complete-command -menu hyprland-focus client

alias global focus hyprland-focus

}
