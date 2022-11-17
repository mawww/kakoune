
provide-module tilix %{

# ensure that we're running on kitty
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ -n "$TILIX_ID" ] || echo 'fail Tilix not detected'
}

define-command -hidden -params 2.. tilix-cmd %{
    evaluate-commands %sh{
        if [ -z "$TILIX_ID" ]; then
            echo "fail 'This command is only available in a Tilix session'"
            exit
        fi
        tilix "$@" < /dev/null > /dev/null 2>&1 &
    }
}

define-command tilix-terminal -params 0.. -docstring '
tilix-terminal-vertical [<program> <arguments>]: create a new Tilix window
The program, if provided, will be executed in the new terminal' \
%{
    tilix-cmd -a app-new-window -e %arg{@}
}
complete-command tilix-terminal shell

define-command tilix-terminal-session -params 0.. -docstring '
tilix-terminal-vertical [<program> <arguments>]: create a new Tilix session in the current window
The program, if provided, will be executed in the new terminal' \
%{
    tilix-cmd -a app-new-session -e %arg{@}
}
complete-command tilix-terminal shell

define-command tilix-terminal-vertical -params 0.. -docstring '
tilix-terminal-vertical [<program> <arguments>]: split tilix terminal vertically
The program, if provided, will be executed in the new terminal' \
%{
    tilix-cmd -a session-add-down -e %arg{@}
}
complete-command tilix-terminal-vertical shell

define-command tilix-terminal-horizontal -params 0.. -docstring '
tilix-terminal-vertical [<program> <arguments>]: split tilix terminal horizontally
The program, if provided, will be executed in the new terminal' \
%{
    tilix-cmd -a session-add-right -e %arg{@}
}
complete-command tilix-terminal-horizontal shell

define-command tilix-terminal-focus -params ..1 -docstring '
tilix-terminal-focus [<kakoune_client>]: focus a given client''s window
If no client is passed, then the current client is used' \
%{
    evaluate-commands %sh{
        if [ $# -eq 1 ]; then
            printf "evaluate-commands -client '%s' focus" "$1"
        else
            tilix --focus-window > /dev/null ||
            echo 'fail failed to run x11-focus, see *debug* buffer for details'
        fi
    }
}
complete-command -menu tilix-terminal-focus client 

alias global focus tilix-terminal-focus
alias global terminal tilix-terminal-horizontal

}
