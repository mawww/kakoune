# https://ghostty.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Ghostty has some bugs that affect this module:
# - Commands that return fast are treated as failing. `sleep 0.1s` fails but `sleep 0.5s` is fine.
#   - As at 2026-07-16 this error is less frightening, but it still happens.
# - 'wait after command false' does not work: it still waits for a keypress.

provide-module ghostty %{

# ensure that we're running in Ghostty
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ "$TERM" = "xterm-ghostty" ] || echo 'fail Ghostty not detected'
    [ -x "$(which osascript)" ] || echo 'fail Ghostty detected; could not find osascript (macOS only)'
}
define-command -params 2.. -docstring '
ghostty-terminal-impl <direction> <program> [<arguments>]
direction is tab|window|up|down|left|right' ghostty-terminal-impl %{
    nop %sh{
        direction="$1"
        shift
        # join the arguments as one string for the shell execution (see x11.kak)
        args=$(
            for i in "$@"; do
                printf "'%s' " "$(printf %s "$i" | sed "s|'|'\\\\''|g")"
            done
        )

        # go through another round of escaping for osascript (see iterm.kak)
        # \ -> \\
        # " -> \"
        do_esc() {
            printf %s "$*" | sed -e 's|\\|\\\\|g; s|"|\\"|g'
        }

        escaped=$(do_esc "$args")
        esc_path=$(do_esc "$PATH")
        esc_tmp=$(do_esc "$TMPDIR")
        cmd="env PATH='${esc_path}' TMPDIR='${esc_tmp}' $escaped"
        if [ "$direction" = 'tab' ]; then
            osascript                                                             \
            -e "tell application \"Ghostty\""                                     \
            -e "    set cfg to new surface configuration"                         \
            -e "    set command of cfg to \"${cmd}\""                             \
            -e "    set wait after command of cfg to false"                       \
            -e "    set newtab to new tab in front window with configuration cfg" \
            -e "    select tab newtab"                                            \
            -e "end tell" >/dev/null
        elif [ "$direction" = 'window' ]; then
            osascript                                                \
            -e "tell application \"Ghostty\""                        \
            -e "    set cfg to new surface configuration"            \
            -e "    set command of cfg to \"${cmd}\""                \
            -e "    set wait after command of cfg to false"          \
            -e "    set newwin to new window with configuration cfg" \
            -e "    activate window newwin"                          \
            -e "end tell" >/dev/null
        else
            osascript                                                                         \
            -e "tell application \"Ghostty\""                                                 \
            -e "    set cfg to new surface configuration"                                     \
            -e "    set command of cfg to \"${cmd}\""                                         \
            -e "    set wait after command of cfg to false"                                   \
            -e "    set term to focused terminal of selected tab of front window"             \
            -e "    set newsplit to split term direction ${direction} with configuration cfg" \
            -e "    focus newsplit"                                                           \
            -e "end tell" >/dev/null
        fi
      }
}
define-command ghostty-terminal-vertical -override -params 1.. -docstring '
ghostty-terminal-vertical <program> [<arguments>]: create a new terminal as an ghostty pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal'\
%{
    ghostty-terminal-impl 'right' %arg{@}
}
complete-command ghostty-terminal-vertical shell

define-command ghostty-terminal-horizontal -override -params 1.. -docstring '
ghostty-terminal-horizontal <program> [<arguments>]: create a new terminal as an ghostty pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal'\
%{
    ghostty-terminal-impl 'down' %arg{@}
}
complete-command ghostty-terminal-horizontal shell

define-command ghostty-terminal-tab -override -params 1.. -docstring '
ghostty-terminal-tab <program> [<arguments>]: create a new terminal as an ghostty tab
The program passed as argument will be executed in the new terminal'\
%{
    ghostty-terminal-impl 'tab' %arg{@}
}
complete-command ghostty-terminal-tab shell

define-command ghostty-terminal-window -override -params 1.. -docstring '
ghostty-terminal-window <program> [<arguments>]: create a new terminal as an ghostty window
The program passed as argument will be executed in the new terminal'\
%{
    ghostty-terminal-impl 'window' %arg{@}
}
complete-command ghostty-terminal-window shell

define-command ghostty-focus -override -params ..1 -docstring '
ghostty-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{ evaluate-commands %sh{
  set -x
    if [ $# -eq 1 ]; then
        printf "evaluate-commands -client '%s' focus" "$1"
    else
        # Works on Ghostty commit 73534c4680a809398b396c94ac7f12fcccb7963d (2026-07-16).
        # Expected to be officially supported beginning with Ghostty 1.4.0.
        THIS_TTY=$(ps -p "$kak_client_pid" -o tty=)
        osascript                                                       \
        -e 'tell application "Ghostty"'                                 \
        -e '  repeat with term in terminals'                            \
        -e '    if tty of term as text = "/dev/'"${THIS_TTY% }"'" then' \
        -e '      focus term'                                           \
        -e '      exit'                                                 \
        -e '    end if'                                                 \
        -e '  end repeat'                                               \
        -e 'end tell' >/dev/null
    fi
}}
complete-command -menu ghostty-focus client

alias global focus ghostty-focus
}

