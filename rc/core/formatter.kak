decl str formatcmd ""
def format -docstring "Format the entire buffer with an external utility" %{
    %sh{
        if [ ! -z "${kak_opt_formatcmd}" ]; then
            ## Save the current position of the cursor
            readonly x=$((kak_cursor_column - 1))
            readonly y="${kak_cursor_line}"

            printf %s\\n "exec -draft %{%|${kak_opt_formatcmd// /<space>}<ret>}"
            ## Try to restore the position of the cursor as it was prior to formatting
            printf %s\\n "exec gg ${y}g ${x}l"
        fi
    }
}
