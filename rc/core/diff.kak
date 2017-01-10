hook global BufCreate .*\.(diff|patch) %{
    set buffer filetype diff
}

add-highlighter -group / group diff
add-highlighter -group /diff regex "^\+[^\n]*\n" 0:green,default
add-highlighter -group /diff regex "^-[^\n]*\n" 0:red,default
add-highlighter -group /diff regex "^@@[^\n]*@@" 0:cyan,default

hook -group diff-highlight global WinSetOption filetype=diff %{ add-highlighter ref diff }
hook -group diff-highlight global WinSetOption filetype=(?!diff).* %{ remove-highlighter diff }
