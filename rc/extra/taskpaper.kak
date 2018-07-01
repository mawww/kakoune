# https://www.taskpaper.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.taskpaper %{
    set-option buffer filetype taskpaper
} 

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/taskpaper group

add-highlighter shared/taskpaper/ regex ^\h*([^:\n]+):\h*\n 1:header
add-highlighter shared/taskpaper/ regex \h@\w+(?:\(([^)]*)\))? 0:variable 1:value
add-highlighter shared/taskpaper/ regex ^\h*([^-:\n]+)\n 1:+i
add-highlighter shared/taskpaper/ regex ^\h*-\h+[^\n]*@done[^\n]* 0:+d
add-highlighter shared/taskpaper/ regex (([a-z]+://\S+)|((mailto:)[\w+-]+@\S+)) 0:link

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden taskpaper-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        ## If the line above is a project indent with a tab
        try %{ execute-keys -draft Z k<a-x> <a-k>^\h*([^:\n]+):<ret> z i<tab> }
        # cleanup trailing white spaces on previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group taskpaper-highlight global WinSetOption filetype=taskpaper %{
    add-highlighter window/taskpaper ref taskpaper
    hook window InsertChar \n -group taskpaper-indent taskpaper-indent-on-new-line
}
hook -group taskpaper-highlight global WinSetOption filetype=(?!taskpaper).* %{
    remove-highlighter window/taskpaper
    remove-hooks window taskpaper-indent
}
