hook global BufCreate .+\.(repo|ini|cfg|properties) %{
    set-option buffer filetype ini
}

add-highlighter shared/ini regions
add-highlighter shared/ini/code default-region group
add-highlighter shared/ini/comment region '(^|\h)\K[#;]' $ fill comment

add-highlighter shared/ini/code/ regex "^\h*\[[^\]]*\]" 0:title
add-highlighter shared/ini/code/ regex "^\h*([^\[][^=\n]*)=([^\n]*)" 1:variable 2:value

hook -group ini-highlight global WinSetOption filetype=ini %{ add-highlighter window/ini ref ini }
hook -group ini-highlight global WinSetOption filetype=(?!ini).* %{ remove-highlighter window/ini }
