hook global BufCreate .*\.(conf) %{
    set-option buffer filetype nginx
}

hook global WinCreate .+\.ini %{
    try %{
        execute-keys /^\h*#<ret>
        set-option buffer filetype nginx
    }
}

hook global WinSetOption filetype=nginx %{
    require-module nginx
}

hook -group nginx-highlight global WinSetOption filetype=nginx %{
    add-highlighter window/nginx ref nginx
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/nginx }
}

provide-module nginx %{

add-highlighter shared/nginx regions
add-highlighter shared/nginx/code default-region group
add-highlighter shared/nginx/comment region '(^|\h)\K#' $ fill comment

add-highlighter shared/nginx/code/ regex "(?S)^\h*(\[.+?\])\h*$" 1:title
add-highlighter shared/nginx/code/ regex "^\h*([^\[][^=\n]*)=([^\n]*)" 1:variable 2:value

}
