hook global BufCreate .*\.(diff|patch) %{
    set-option buffer filetype diff
}

set-face global diffadd green
set-face global diffremove red
set-face global diffmeta cyan

add-highlighter shared/diff group
add-highlighter shared/diff/ regex "^\+[^\n]*\n" 0:diffadd
add-highlighter shared/diff/ regex "^-[^\n]*\n" 0:diffremove
add-highlighter shared/diff/ regex "^@@[^\n]*@@" 0:diffmeta

hook -group diff-highlight global WinSetOption filetype=diff %{ add-highlighter window/diff ref diff }
hook -group diff-highlight global WinSetOption filetype=(?!diff).* %{ remove-highlighter window/diff }
