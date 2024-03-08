# macOS Terminal.app
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

provide-module appleterminal %{

# ensure that we're running in Terminal.app
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || [ "$TERM_PROGRAM" = "Apple_Terminal" ] || echo 'fail Terminal.app not detected'
}

define-command appleterminal-terminal-window -params 1.. -docstring '
appleterminal-terminal-window <program> [<arguments>]: create a new terminal as a Terminal.app window
The program passed as argument will be executed in the new terminal'\
%{
    nop %sh{
        # join the arguments as one string for the shell execution (see x11.kak)
        quoted=$(
            for i in exec env "PATH=$PATH" "TMPDIR=$TMPDIR" "$@"; do
                printf "'%s' " "$(printf %s "$i" | sed "s|'|'\\\\''|g")"
            done
        )

        # since Terminal.app runs the command in the interactive shell, add initial space to prevent adding it to shell history
        cmd=" $quoted"
        echo "$cmd" | osascript                             \
            -e 'set s to (do shell script "cat 0<&3")'      \
            -e 'tell application "Terminal" to do script s' >/dev/null 3<&0
    }
}
complete-command appleterminal-terminal-window shell

alias global terminal appleterminal-terminal-window

define-command appleterminal-focus -params ..1 -docstring '
appleterminal-focus [<client>]: focus the given client
If no client is passed then the current one is used' \
%{
    fail 'Focusing Terminal.app client is unimplemented'
}
complete-command -menu appleterminal-focus client

alias global focus appleterminal-focus

}
