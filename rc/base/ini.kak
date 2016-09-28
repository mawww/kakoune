hook global BufCreate .*\.(repo|service|target|socket|ini|cfg) %{
    set buffer filetype ini
}

addhl -group / regions -default code ini \
    comment (^|\h)\K\# $ ''

addhl -group /ini/code regex "^\h*\[[^\]]*\]" 0:title
addhl -group /ini/code regex "^\h*([^\[][^=\n]*=)([^\n]*)" 1:identifier 2:value

addhl -group /ini/comment fill comment

hook global WinSetOption filetype=ini %{
    addhl ref ini
}

hook global WinSetOption filetype=(?!ini).* %{
    rmhl ini
}
