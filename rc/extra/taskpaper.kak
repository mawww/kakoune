# https://www.taskpaper.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.taskpaper %{
    set buffer filetype taskpaper
} 

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / group taskpaper

addhl -group /taskpaper regex ^\h*([^:\n]+):\h*\n 1:header
addhl -group /taskpaper regex \h@\w+(?:\(([^)]*)\))? 0:identifier 1:value
addhl -group /taskpaper regex ^\h*([^-:\n]+)\n 1:+i
addhl -group /taskpaper regex ^\h*-\h+[^\n]*@done[^\n]* 0:+d
addhl -group /taskpaper regex (([a-z]+://\S+)|((mailto:)[\w+-]+@\S+)) 0:link

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _taskpaper-indent-on-new-line %{
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
    addhl ref taskpaper
    hook window InsertChar \n -group taskpaper-indent _taskpaper-indent-on-new-line
}
hook -group taskpaper-highlight global WinSetOption filetype=(?!taskpaper).* %{
    rmhl taskpaper
    rmhooks window taskpaper-indent
}
