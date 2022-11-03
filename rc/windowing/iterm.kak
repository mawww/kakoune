# https://www.iterm2.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module iterm %{

# ensure that we're running on iTerm
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ "$TERM_PROGRAM" = "iTerm.app" ] || echo 'fail iTerm not detected'
}

define-command -hidden -params 2.. iterm-terminal-split-impl %{
    nop %sh{
        direction="$1"
        shift
        # join the arguments as one string for the shell execution (see x11.kak)
        args=$(
            for i in "$@"; do
                if [ "$i" = '' ]; then
                    printf "'' "
                else
                    printf %s "$i" | sed -e "s|'|'\\\\''|g; s|^|'|; s|$|' |"
                fi
            done
        )

        # go through another round of escaping for osascript
        # \ -> \\
        # " -> \"
        do_esc() {
            printf %s "$*" | sed -e 's|\\|\\\\|g; s|"|\\"|g'
        }

        escaped=$(do_esc "$args")
        esc_path=$(do_esc "$PATH")
        esc_tmp=$(do_esc "$TMPDIR")
        cmd="env PATH='${esc_path}' TMPDIR='${esc_tmp}' $escaped"
        osascript                                                                             \
        -e "tell application \"iTerm\""                                                       \
        -e "    tell current session of current window"                                       \
        -e "        tell (split ${direction} with same profile command \"${cmd}\") to select" \
        -e "    end tell"                                                                     \
        -e "end tell" >/dev/null
    }
}

define-command iterm-terminal-vertical -params 1.. -docstring '
iterm-terminal-vertical <program> [<arguments>]: create a new terminal as an iterm pane
The current pane is split into two, left and right
The program passed as argument will be executed in the new terminal'\
%{
    iterm-terminal-split-impl 'vertically' %arg{@}
}
complete-command iterm-terminal-vertical shell

define-command iterm-terminal-horizontal -params 1.. -docstring '
iterm-terminal-horizontal <program> [<arguments>]: create a new terminal as an iterm pane
The current pane is split into two, top and bottom
The program passed as argument will be executed in the new terminal'\
%{
    iterm-terminal-split-impl 'horizontally' %arg{@}
}
complete-command iterm-terminal-horizontal shell

define-command iterm-terminal-tab -params 1.. -docstring '
iterm-terminal-tab <program> [<arguments>]: create a new terminal as an iterm tab
The program passed as argument will be executed in the new terminal'\
%{
    nop %sh{
        # see above
        args=$(
            for i in "$@"; do
                if [ "$i" = '' ]; then
                    printf "'' "
                else
                    printf %s "$i" | sed -e "s|'|'\\\\''|g; s|^|'|; s|$|' |"
                fi
            done
        )
        escaped=$(printf %s "$args" | sed -e 's|\\|\\\\|g; s|"|\\"|g')
        cmd="env PATH='${PATH}' TMPDIR='${TMPDIR}' $escaped"
        osascript                                                       \
        -e "tell application \"iTerm\""                                 \
        -e "    tell current window"                                    \
        -e "        create tab with default profile command \"${cmd}\"" \
        -e "    end tell"                                               \
        -e "end tell" >/dev/null
    }
}
complete-command iterm-terminal-tab shell

define-command iterm-terminal-window -params 1.. -docstring '
iterm-terminal-window <program> [<arguments>]: create a new terminal as an iterm window
The program passed as argument will be executed in the new terminal'\
%{
    nop %sh{
        # see above
        args=$(
            for i in "$@"; do
                if [ "$i" = '' ]; then
                    printf "'' "
                else
                    printf %s "$i" | sed -e "s|'|'\\\\''|g; s|^|'|; s|$|' |"
                fi
            done
        )
        escaped=$(printf %s "$args" | sed -e 's|\\|\\\\|g; s|"|\\"|g')
        cmd="env PATH='${PATH}' TMPDIR='${TMPDIR}' $escaped"
        osascript                                                      \
        -e "tell application \"iTerm\""                                \
        -e "    create window with default profile command \"${cmd}\"" \
        -e "end tell" >/dev/null
    }
}
complete-command iterm-terminal-window shell

define-command iterm-focus -params ..1 -docstring '
iterm-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf %s\\n "evaluate-commands -client '$1' focus"
        else
            session="${kak_client_env_ITERM_SESSION_ID#*:}"
            osascript                                                      \
            -e "tell application \"iTerm\" to repeat with aWin in windows" \
            -e "    tell aWin to repeat with aTab in tabs"                 \
            -e "        tell aTab to repeat with aSession in sessions"     \
            -e "            tell aSession"                                 \
            -e "                if (unique id = \"${session}\") then"      \
            -e "                    tell aWin"                             \
            -e "                        select"                            \
            -e "                    end tell"                              \
            -e "                    tell aTab"                             \
            -e "                        select"                            \
            -e "                    end tell"                              \
            -e "                    select"                                \
            -e "                end if"                                    \
            -e "            end tell"                                      \
            -e "        end repeat"                                        \
            -e "    end repeat"                                            \
            -e "end repeat"
        fi
    }
}
complete-command -menu iterm-focus client

alias global focus iterm-focus
alias global terminal iterm-terminal-vertical

}
