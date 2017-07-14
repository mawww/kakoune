decl -docstring "shell command to which the contents of the current buffer is piped" \
    str formatcmd

# This option holds the selection as it was at the time of calling the `format` command
# After the buffer has been formatted, the selection will be restored according to
# the value held in this option
decl -hidden range-specs formatter_prevsel

def format -docstring "Format the contents of the current buffer" %{
    set buffer formatter_prevsel "%val{timestamp}"
    eval -itersel %{
        set -add buffer formatter_prevsel "%val{selection_desc}|Default"
    }

    eval -draft %{ %sh{
        if [ -n "${kak_opt_formatcmd}" ]; then
            path_file_tmp=$(mktemp "${TMPDIR:-/tmp}"/kak-formatter-XXXXXX)
            printf %s\\n "
                write \"${path_file_tmp}\"

                %sh{
                    readonly path_file_out=\$(mktemp \"${TMPDIR:-/tmp}\"/kak-formatter-XXXXXX)

                    if cat \"${path_file_tmp}\" | eval \"${kak_opt_formatcmd}\" > \"\${path_file_out}\"; then
                        printf '%s\\n' \"
                            exec \\%|cat<space>'\${path_file_out}'<ret>
                            %sh{ rm -f '\${path_file_out}' }
                            update-option buffer formatter_prevsel
                            eval -client '${kak_client}' select %sh{ printf %s \\\"\${kak_opt_formatter_prevsel#*:}\\\" | sed 's/|.*//' }
                        \"
                    else
                        printf '%s\\n' \"
                            eval -client '${kak_client}' echo -color Error formatter returned an error (\$?)
                        \"
                        rm -f \"\${path_file_out}\"
                    fi

                    rm -f \"${path_file_tmp}\"
                }
            "
        else
            printf '%s\n' "eval -client '${kak_client}' echo -color Error formatcmd option not specified"
        fi
    } }
}
