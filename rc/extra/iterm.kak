# https://www.iterm2.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

## The default behaviour for the `new` command is to open an vertical pane in
## an iTerm session if not in a tmux session.
hook global KakBegin .* %{
    %sh{
        if [ "$TERM_PROGRAM" = "iTerm.app" -a -z "$TMUX" ]; then
            echo "
                alias global new iterm-new-vertical
                alias global focus iterm-focus
            "
        fi
    }
}

define-command -hidden -params 1.. iterm-new-split-impl %{
    %sh{
        direction="$1"
        shift
        if [ $# -gt 0 ]; then kakoune_params="-e '$@'"; fi
        sh_cmd="kak -c ${kak_session} ${kakoune_params}"
        osascript                                                     \
        -e "tell application \"iTerm\""                               \
        -e "    tell current session of current window"               \
        -e "        tell (split ${direction} with same profile)"      \
        -e "            select"                                       \
        -e "            write text \"export TMPDIR='${TMPDIR}'\""     \
        -e "            write text \"exec ${sh_cmd}\""                \
        -e "        end tell"                                         \
        -e "    end tell"                                             \
        -e "end tell"
    }
}

define-command iterm-new-vertical -params .. -command-completion -docstring "Create a new vertical pane" %{
    iterm-new-split-impl 'vertically' %arg{@}
}

define-command iterm-new-horizontal -params .. -command-completion -docstring "Create a new horizontal pane" %{
    iterm-new-split-impl 'horizontally' %arg{@}
}

define-command -params .. -command-completion \
    -docstring %{iterm-new-tab [<arguments>]: create a new tab
All optional arguments are forwarded to the new kak client} \
    iterm-new-tab %{
    %sh{
        if [ $# -gt 0 ]; then kakoune_params="-e '$@'"; fi
        sh_cmd="kak -c ${kak_session} ${kakoune_params}"
        osascript                                                              \
        -e "tell application \"iTerm\""                                        \
        -e "    tell current window"                                           \
        -e "        tell current session of (create tab with default profile)" \
        -e "            write text \"export TMPDIR='${TMPDIR}'\""              \
        -e "            write text \"exec ${sh_cmd}\""                         \
        -e "        end tell"                                                  \
        -e "    end tell"                                                      \
        -e "end tell"
    }
}

define-command -params .. -command-completion \
    -docstring %{iterm-new-window [<arguments>]: create a new window
All optional arguments are forwarded to the new kak client} \
    iterm-new-window %{
    %sh{
        if [ $# -gt 0 ]; then kakoune_params="-e '$@'"; fi
        sh_cmd="kak -c ${kak_session} ${kakoune_params}"
        osascript                                                 \
        -e "tell application \"iTerm\""                           \
        -e "    set-option w to (create window with default profile)"    \
        -e "    tell current session of w"                        \
        -e "        write text \"export TMPDIR='${TMPDIR}'\""     \
        -e "        write text \"exec ${sh_cmd}\""                \
        -e "    end tell"                                         \
        -e "end tell"
    }
}

define-command -params ..1 -client-completion \
    -docstring %{iterm-focus [<client>]: focus the given client
If no client is passed then the current one is used} \
    iterm-focus %{
    # Should be possible using ${kak_client_env_ITERM_SESSION_ID}.
     %sh{echo "echo -markup '{Error}Not implemented yet for iTerm'"}
}
