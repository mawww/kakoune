hook global BufCreate .*\.(repo|service|target|socket|ini|cfg) %{
    set buffer filetype ini
}

add-highlighter -group / regions -default code ini \
    comment (^|\h)\K\# $ ''

add-highlighter -group /ini/code regex "^\h*\[[^\]]*\]" 0:title
add-highlighter -group /ini/code regex "^\h*([^\[][^=\n]*=)([^\n]*)" 1:variable 2:value

add-highlighter -group /ini/comment fill comment

hook -group ini-highlight global WinSetOption filetype=ini %{ add-highlighter ref ini }
hook -group ini-highlight global WinSetOption filetype=(?!ini).* %{ remove-highlighter ini }
