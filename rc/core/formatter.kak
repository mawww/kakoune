decl str formatcmd ""
def format -docstring "Format the contents of the current buffer" %{
    %sh{
        if [ ! -z "${kak_opt_formatcmd}" ]; then
            readonly kak_opt_formatcmd=$(printf '%s' "${kak_opt_formatcmd}" | sed 's/ /<space>/g')
            ## Save the current position of the cursor
            readonly x=$((kak_cursor_column - 1))
            readonly y="${kak_cursor_line}"

            printf %s\\n "exec -draft %{%|${kak_opt_formatcmd}<ret>}"
            ## Try to restore the position of the cursor as it was prior to formatting
            printf %s\\n "exec gg ${y}g ${x}l"
        fi
    }
}
