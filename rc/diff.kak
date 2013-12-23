hook global BufCreate .*\.(diff|patch) %{
    set buffer filetype diff
}

hook global WinSetOption filetype=diff %{
    addhl group diff-highlight
    addhl -group diff-highlight regex "^\+[^\n]*\n" 0:green,default
    addhl -group diff-highlight regex "^-[^\n]*\n" 0:red,default
    addhl -group diff-highlight regex "^@@[^\n]*@@" 0:cyan,default
}

hook global WinSetOption filetype=(?!diff).* %{
    rmhl diff-highlight
}
