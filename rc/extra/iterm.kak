# https://www.iterm2.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## The default behaviour for the `new` command is to open an vertical pane in
## an iTerm session if not in a tmux session.
hook global KakBegin .* %sh{
    if [ "$TERM_PROGRAM" = "iTerm.app" ] && [ -z "$TMUX" ]; then
        echo "
            alias global new iterm-new-vertical
            alias global focus iterm-focus
        "
    fi
}

define-command -hidden -params 1.. iterm-new-split-impl %{
    nop %sh{
        direction="$1"
        shift
        if [ $# -gt 0 ]; then kakoune_params="-e \\\"$*\\\""; fi
        cmd="env PATH='${PATH}' TMPDIR='${TMPDIR}' kak -c '${kak_session}' ${kakoune_params}"
        osascript                                                                             \
        -e "tell application \"iTerm\""                                                       \
        -e "    tell current session of current window"                                       \
        -e "        tell (split ${direction} with same profile command \"${cmd}\") to select" \
        -e "    end tell"                                                                     \
        -e "end tell" >/dev/null
    }
}

define-command iterm-new-vertical -params .. -command-completion -docstring "Split the current pane into two, top and bottom" %{
    iterm-new-split-impl 'vertically' %arg{@}
}

define-command iterm-new-horizontal -params .. -command-completion -docstring "Split the current pane into two, left and right" %{
    iterm-new-split-impl 'horizontally' %arg{@}
}

define-command -params .. -command-completion \
    -docstring %{iterm-new-tab [<arguments>]: create a new tab
All optional arguments are forwarded to the new kak client} \
    iterm-new-tab %{
    nop %sh{
        if [ $# -gt 0 ]; then kakoune_params="-e \\\"$*\\\""; fi
        cmd="env PATH='${PATH}' TMPDIR='${TMPDIR}' kak -c '${kak_session}' ${kakoune_params}"
        osascript                                                       \
        -e "tell application \"iTerm\""                                 \
        -e "    tell current window"                                    \
        -e "        create tab with default profile command \"${cmd}\"" \
        -e "    end tell"                                               \
        -e "end tell" >/dev/null
    }
}

define-command -params .. -command-completion \
    -docstring %{iterm-new-window [<arguments>]: create a new window
All optional arguments are forwarded to the new kak client} \
    iterm-new-window %{
    nop %sh{
        if [ $# -gt 0 ]; then kakoune_params="-e \\\"$*\\\""; fi
        cmd="env PATH='${PATH}' TMPDIR='${TMPDIR}' kak -c '${kak_session}' ${kakoune_params}"
        osascript                                                      \
        -e "tell application \"iTerm\""                                \
        -e "    create window with default profile command \"${cmd}\"" \
        -e "end tell" >/dev/null
    }
}

define-command -params ..1 -client-completion \
    -docstring %{iterm-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    iterm-focus %{ evaluate-commands %sh{
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
            -e "                    select"                                \
            -e "                end if"                                    \
            -e "            end tell"                                      \
            -e "        end repeat"                                        \
            -e "    end repeat"                                            \
            -e "end repeat"
        fi
    }
}
