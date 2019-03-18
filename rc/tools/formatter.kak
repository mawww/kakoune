declare-option -docstring "shell command to which the contents of the current buffer is piped" \
    str formatcmd

define-command format -docstring "Format the contents of the current buffer" %{ evaluate-commands -draft -no-hooks %{
    evaluate-commands %sh{
        if [ -n "${kak_opt_formatcmd}" ]; then
            path_file_tmp=$(mktemp "${TMPDIR:-/tmp}"/kak-formatter-XXXXXX)
            printf %s\\n "
                write -sync \"${path_file_tmp}\"

                evaluate-commands %sh{
                    readonly path_file_out=\$(mktemp \"${TMPDIR:-/tmp}\"/kak-formatter-XXXXXX)

                    if cat \"${path_file_tmp}\" | eval \"${kak_opt_formatcmd}\" > \"\${path_file_out}\"; then
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
