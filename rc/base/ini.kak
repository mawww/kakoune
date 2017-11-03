hook global BufCreate .*\.(repo|service|target|socket|ini|cfg) %{
    set-option buffer filetype ini
}

add-highlighter shared/ regions -default code ini \
    comment '(^|\h)\K[#;]' $ ''

add-highlighter shared/ini/code regex "^\h*\[[^\]]*\]" 0:title
add-highlighter shared/ini/code regex "^\h*([^\[][^=\n]*=)([^\n]*)" 1:variable 2:value

add-highlighter shared/ini/comment fill comment

hook -group ini-highlight global WinSetOption filetype=ini %{ add-highlighter window ref ini }
hook -group ini-highlight global WinSetOption filetype=(?!ini).* %{ remove-highlighter window/ini }
