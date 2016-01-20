hook global BufCreate .*\.(repo|service|target|socket|ini|cfg) %{
    set buffer filetype ini-file
}

addhl -group / regions -default code ini-highlighter \
    comment (^|\h)\K\# $ ''

addhl -group /ini-highlighter/code regex "^\h*\[[^\]]*\]" 0:title
addhl -group /ini-highlighter/code regex "^\h*([^\[][^=\n]*=)([^\n]*)" 1:identifier 2:value

addhl -group /ini-highlighter/comment fill comment

hook global WinSetOption filetype=ini-file %{
    addhl ref ini-highlighter
}

hook global WinSetOption filetype=(?!ini-file).* %{
    rmhl ini-highlighter
}
