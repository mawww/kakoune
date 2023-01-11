hook global BufCreate .+\.ini %{
    set-option buffer filetype ini
}

hook global WinSetOption filetype=ini %{
    require-module ini
}

hook -group ini-highlight global WinSetOption filetype=ini %{
    add-highlighter window/ini ref ini
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ini }
}


provide-module ini %{

add-highlighter shared/ini regions
add-highlighter shared/ini/code default-region group
add-highlighter shared/ini/comment region '(^|\h)\K[#;]' $ fill comment

add-highlighter shared/ini/code/ regex "(?S)^\h*(\[.+?\])\h*$" 1:title
add-highlighter shared/ini/code/ regex "^\h*([^\[][^=\n]*)=([^\n]*)" 1:variable 2:value

}
