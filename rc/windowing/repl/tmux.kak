# http://tmux.github.io/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# Tmux version >= 2 is required to use this module

hook global ModuleLoaded tmux %{
    require-module tmux-repl
}

provide-module tmux-repl %{

declare-option -docstring "tmux pane id in which the REPL is running" str tmux_repl_id

define-command -hidden -params 1.. tmux-repl-impl %{
    evaluate-commands %sh{
        if [ -z "$TMUX" ]; then
            echo 'fail This command is only available in a tmux session'
            exit
        fi
        tmux_args="$1"
        if [ "${1%%-*}" = split ]; then
            tmux_args="$tmux_args -t ${kak_client_env_TMUX_PANE}"
        elif [ "${1%% *}" = new-window ]; then
            session_id=$(tmux display-message -p -t ${kak_client_env_TMUX_PANE} '#{session_id}')
            tmux_args="$tmux_args -t $session_id"
        fi
        shift
        repl_pane_id=$(tmux $tmux_args -P -F '#{pane_id}' "$@")
        printf "set-option current tmux_repl_id '%s'" "$repl_pane_id"
    }
}

define-command tmux-repl-vertical -params 0.. -docstring "Create a new vertical pane for repl interaction" %{
    tmux-repl-impl 'split-window -v' %arg{@}
}
complete-command tmux-repl-vertical shell

define-command tmux-repl-horizontal -params 0.. -docstring "Create a new horizontal pane for repl interaction" %{
    tmux-repl-impl 'split-window -h' %arg{@}
}
complete-command tmux-repl-horizontal shell

define-command tmux-repl-window -params 0.. -docstring "Create a new window for repl interaction" %{
    tmux-repl-impl 'new-window' %arg{@}
}
complete-command tmux-repl-window shell

define-command -params 0..1 tmux-repl-set-pane -docstring %{
        tmux-repl-set-pane [pane number]: Set an existing tmux pane for repl interaction
        If the address of new pane is not given, next pane is used
        (To get the pane number in tmux,
        use 'tmux display-message -p '#{pane_id}'" in that pane)
    } %{
    evaluate-commands %sh{
        if [ -z "$TMUX" ]; then
            echo 'fail This command is only available in a tmux session'
            exit
        fi
        if [ $# -eq 0 ]; then
            curr_pane_no="${kak_client_env_TMUX_PANE#%}"
            tgt_pane=$((curr_pane_no+1))
        else
            tgt_pane="$1"
        fi
        curr_win="$(tmux display-message -t ${kak_client_env_TMUX_PANE} -p '#{window_id}')" 
        if tmux list-panes -t "$curr_win" -F \#D | grep -Fxq "%"$tgt_pane; then
            printf "set-option current tmux_repl_id '%s'" %$tgt_pane
        else
            echo 'fail The correct pane is not there. Activate using tmux-terminal-* or some other way'
        fi
    }
}

define-command -hidden tmux-send-text -params 0..1 -docstring %{
        tmux-send-text [text]: Send text to the REPL pane.
        If no text is passed, then the selection is used
    } %{
    evaluate-commands %sh{
        if [ $# -eq 0 ]; then
            tmux set-buffer -b kak_selection -- "${kak_selection}"
        else
            tmux set-buffer -b kak_selection -- "$1"
        fi
        tmux paste-buffer -b kak_selection -t "$kak_opt_tmux_repl_id" ||
        echo 'fail tmux-send-text: failed to send text, see *debug* buffer for details'
    }
}

alias global repl-new tmux-repl-horizontal
alias global repl-send-text tmux-send-text

}
