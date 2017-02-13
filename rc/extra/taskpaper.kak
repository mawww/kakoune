# https://www.taskpaper.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.taskpaper %{
    set buffer filetype taskpaper
} 

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / group taskpaper

add-highlighter -group /taskpaper regex ^\h*([^:\n]+):\h*\n 1:header
add-highlighter -group /taskpaper regex \h@\w+(?:\(([^)]*)\))? 0:variable 1:value
add-highlighter -group /taskpaper regex ^\h*([^-:\n]+)\n 1:+i
add-highlighter -group /taskpaper regex ^\h*-\h+[^\n]*@done[^\n]* 0:+d
add-highlighter -group /taskpaper regex (([a-z]+://\S+)|((mailto:)[\w+-]+@\S+)) 0:link

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden taskpaper-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft \;K<a-&> }
        ## If the line above is a project indent with a tab
        try %{ exec -draft Z k<a-x> <a-k>^\h*([^:\n]+):<ret> z i<tab> }
        # cleanup trailing white spaces on previous line
        try %{ exec -draft k<a-x> s \h+$ <ret>d }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group taskpaper-highlight global WinSetOption filetype=taskpaper %{
    add-highlighter ref taskpaper
    hook window InsertChar \n -group taskpaper-indent taskpaper-indent-on-new-line
}
hook -group taskpaper-highlight global WinSetOption filetype=(?!taskpaper).* %{
    remove-highlighter taskpaper
    remove-hooks window taskpaper-indent
}
