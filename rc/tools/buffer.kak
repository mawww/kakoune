provide-module buffer %§§

declare-option -docstring "buffer minor mode" str buffer_kind

define-command -hidden buffer-with-last -params 2.. -docstring %{
    buffer-with-last <kind> <cmd> [<arguments>]: run the given command,
    passing as final argument the last buffer whose 'buffer_kind' option is
    set to <kind>
} %{
    evaluate-commands %sh{
        kind=$1
        shift
        cmd=$*
        eval set -- "${kak_quoted_buflist}"
        i=$#
        while [ $i -gt 0 ]; do
            eval bufname=\${$i}
            echo >${kak_command_fifo} "evaluate-commands -buffer $bufname %{
                echo -to-file ${kak_response_fifo} %opt{buffer_kind}
            }"
            if [ "$(cat <${kak_response_fifo})" = "${kind}" ]; then
                echo "$cmd '$(printf %s "$bufname" | sed "s/'/''/g")'"
                exit
            fi
            i=$(( $i - 1))
        done
        echo "fail no buffer of kind %arg{1}"
    }
}
complete-command buffer-with-last shell-script-candidates %{ echo jump }
