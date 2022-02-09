hook global BufCreate .+\.(repo|cfg|properties|desktop) %{
    set-option buffer filetype conf
}

hook global WinCreate .+\.ini %{
    try %{
        execute-keys /^\h*#<ret>
        set-option buffer filetype conf
    }
}

hook global WinSetOption filetype=conf %{
    require-module conf
}

hook -group conf-highlight global WinSetOption filetype=conf %{
    add-highlighter window/conf ref conf
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/conf }
}

provide-module conf %{

add-highlighter shared/conf regions
add-highlighter shared/conf/code default-region group
add-highlighter shared/conf/comment region '(^|\h)\K#' $ fill comment

add-highlighter shared/conf/code/ regex "(?S)^\h*(\[.+?\])\h*$" 1:title
add-highlighter shared/conf/code/ regex "^\h*([^\[][^=\n]*)=([^\n]*)" 1:variable 2:value

}
