declare-option -docstring %{
    shell command running an external formatting tool to format the current buffer

The following environment variables are available:
  - format_file_in: path to a copy of the current buffer
  - format_file_out: path to the file that should be filled with the buffer's formatted data
    } \
    str formatcmd

define-command format -docstring "Format the contents of the current buffer" %{ evaluate-commands -draft -no-hooks %{
    evaluate-commands %sh{
        if [ -n "${kak_opt_formatcmd}" ]; then
            path_file_tmp=$(mktemp "${TMPDIR:-/tmp}"/kak-formatter-XXXXXX)
            printf %s\\n "
                write -sync \"${path_file_tmp}\"

                evaluate-commands %sh{
                    readonly path_file_out=\$(mktemp \"${TMPDIR:-/tmp}\"/kak-formatter-XXXXXX)

                    export format_file_in=\"${path_file_tmp}\"
                    export format_file_out=\"\${path_file_out}\"

                    if eval \"${kak_opt_formatcmd}\"; then
                        printf '%s\\n' \"execute-keys \\%|cat<space>'\${path_file_out}'<ret>\"
                        printf '%s\\n' \"nop %sh{ rm -f '\${path_file_out}' }\"
                    else
                        printf '%s\\n' \"
                            evaluate-commands -client '${kak_client}' echo -markup '{Error}formatter returned an error (\$?)'
                        \"
                        rm -f \"\${path_file_out}\"
                    fi

                    rm -f \"${path_file_tmp}\"
                }
            "
        else
            printf '%s\n' "evaluate-commands -client '${kak_client}' echo -markup '{Error}formatcmd option not specified'"
        fi
    }
} }
