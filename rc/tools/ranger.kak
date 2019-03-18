# http://ranger.nongnu.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

define-command ranger-open-on-edit-directory \
    -docstring 'Start the ranger file system explorer when trying to edit a directory' %{
        hook global RuntimeError "\d+:\d+: '\w+' (.*): is a directory" %{ evaluate-commands %sh{
            directory=$kak_hook_param_capture_1
            echo ranger $directory
    }}
}

define-command \
    -params .. -file-completion \
    -docstring %{ranger [<arguments>]: open the file system explorer to select buffers to open
    All the optional arguments are forwarded to the ranger utility} \
    ranger %{ evaluate-commands %sh{
    if [ -n "${TMUX}" ]; then
        tmux split-window -h \
            ranger $@ --cmd " \
                map <return> eval \
                    fm.execute_console('shell \
                        echo evaluate-commands -client ' + ranger.ext.shell_escape.shell_escape('$kak_client') + ' edit {file} | \
                        kak -p '.format(file=fm.thisfile.path) + ranger.ext.shell_escape.shell_escape('$kak_session') + '; \
                        tmux select-pane -t $kak_client_env_TMUX_PANE') \
                    if fm.thisfile.is_file else fm.execute_console('move right=1')"

    elif [ -n "${STY}" ]; then

        script="/tmp/kak-ranger-${kak_client}-${kak_session}.sh"
        selections="/tmp/kak-ranger-${kak_client}-${kak_session}.txt"
        cat > "$script" << EOF
#! /usr/bin/env sh
cd "$PWD"
ranger --choosefiles="$selections" $@
while read -r f; do
    printf %s  "evaluate-commands -client '${kak_client}' edit '\"\$f\"'" | kak -p '${kak_session}'
done < "$selections"
screen -X remove
rm -f "$selections" "$script"
EOF

        tty="$(ps -o tty ${kak_client_pid} | tail -n 1)"
        screen -X eval \
            'split -h' \
            'focus down' \
            "screen sh '$script'" \
        < "/dev/$tty"

    elif [ -n "$WINDOWID" ]; then
        setsid $kak_opt_termcmd " \
            ranger $@ --cmd "'"'" \
                map <return> eval \
                    fm.execute_console('shell \
                        echo evaluate-commands -client ' + ranger.ext.shell_escape.shell_escape('$kak_client') + ' edit {file} | \
                        kak -p '.format(file=fm.thisfile.path) + ranger.ext.shell_escape.shell_escape('$kak_session') + '; \
                        xdotool windowactivate $kak_client_env_WINDOWID') \
                    if fm.thisfile.is_file else fm.execute_console('move right=1')"'"' < /dev/null > /dev/null 2>&1 &
    fi
}}
