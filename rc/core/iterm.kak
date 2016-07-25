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

def -hidden -params 1.. iterm-new-split-impl %{
    %sh{
        direction="$1"
        shift
        if [ $# -gt 0 ]; then kakoune_params="-e '$@'"; fi
        sh_cmd="kak -c ${kak_session} ${kakoune_params}"
        osascript                                                \
        -e "tell application \"iTerm\""                          \
        -e "    tell current session of current window"          \
        -e "        tell (split ${direction} with same profile)" \
        -e "            select"                                  \
        -e "            write text \"${sh_cmd}\""                \
        -e "        end tell"                                    \
        -e "    end tell"                                        \
        -e "end tell"
    }
}

def iterm-new-vertical -params .. -command-completion -docstring "Create a new vertical pane in iTerm" %{
    iterm-new-split-impl 'vertically' %arg{@}
}

def iterm-new-horizontal -params .. -command-completion -docstring "Create a new horizontal pane in iTerm" %{
    iterm-new-split-impl 'horizontally' %arg{@}
}

def iterm-new-tab -params .. -command-completion -docstring "Create a new tab in iTerm" %{
    %sh{
        if [ $# -gt 0 ]; then kakoune_params="-e '$@'"; fi
        sh_cmd="kak -c ${kak_session} ${kakoune_params}"
        osascript                                                              \
        -e "tell application \"iTerm\""                                        \
        -e "    tell current window"                                           \
        -e "        tell current session of (create tab with default profile)" \
        -e "            write text \"${sh_cmd}\""                              \
        -e "        end tell"                                                  \
        -e "    end tell"                                                      \
        -e "end tell"
    }
}

def iterm-new-window -params .. -command-completion -docstring "Create a new iTerm window" %{
    %sh{
        if [ $# -gt 0 ]; then kakoune_params="-e '$@'"; fi
        sh_cmd="kak -c ${kak_session} ${kakoune_params}"
        osascript                                              \
        -e "tell application \"iTerm\""                        \
        -e "    set w to (create window with default profile)" \
        -e "    tell current session of w"                     \
        -e "        write text \"${sh_cmd}\""                  \
        -e "    end tell"                                      \
        -e "end tell"
    }
}

def iterm-focus -params 0..1 -client-completion -docstring "Focus the given client in iTerm" %{
    # Should be possible using ${kak_client_env_ITERM_SESSION_ID}.
     %sh{echo "echo -color Error 'Not implemented yet for iTerm'"} 
}
