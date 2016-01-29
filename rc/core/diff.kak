hook global BufCreate .*\.(diff|patch) %{
    set buffer filetype diff
}

addhl -group / group diff
addhl -group /diff regex "^\+[^\n]*\n" 0:green,default
addhl -group /diff regex "^-[^\n]*\n" 0:red,default
addhl -group /diff regex "^@@[^\n]*@@" 0:cyan,default

hook global WinSetOption filetype=diff %{ addhl ref diff }
hook global WinSetOption filetype=(?!diff).* %{ rmhl diff }
