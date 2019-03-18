hook global BufCreate .*\.(diff|patch) %{
    set-option buffer filetype diff
}

add-highlighter shared/diff group
add-highlighter shared/diff/ regex "^\+[^\n]*\n" 0:green,default
add-highlighter shared/diff/ regex "^-[^\n]*\n" 0:red,default
add-highlighter shared/diff/ regex "^@@[^\n]*@@" 0:cyan,default

hook -group diff-highlight global WinSetOption filetype=diff %{
    add-highlighter window/diff ref diff
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/diff }
}
