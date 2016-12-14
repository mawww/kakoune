hook global BufCreate .*\.(diff|patch) %{
    set buffer mimetype ""
    set buffer filetype diff
}

addhl -group / group diff
addhl -group /diff regex "^\+[^\n]*\n" 0:green,default
addhl -group /diff regex "^-[^\n]*\n" 0:red,default
addhl -group /diff regex "^@@[^\n]*@@" 0:cyan,default

hook -group diff-highlight global WinSetOption filetype=diff %{ addhl ref diff }
hook -group diff-highlight global WinSetOption filetype=(?!diff).* %{ rmhl diff }
