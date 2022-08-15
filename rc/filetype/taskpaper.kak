# https://www.taskpaper.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.taskpaper %{
    set-option buffer filetype taskpaper
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=taskpaper %{
    require-module taskpaper

    hook window ModeChange pop:insert:.* -group taskpaper-trim-indent taskpaper-trim-indent
    hook window InsertChar \n -group taskpaper-indent taskpaper-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window taskpaper-.+ }
}

hook -group taskpaper-highlight global WinSetOption filetype=taskpaper %{
    add-highlighter window/taskpaper ref taskpaper
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/taskpaper }
}


provide-module taskpaper %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/taskpaper group

add-highlighter shared/taskpaper/ regex ^\h*([^:\n]+):\h*\n 1:header
add-highlighter shared/taskpaper/ regex \h@\w+(?:\(([^)]*)\))? 0:variable 1:value
add-highlighter shared/taskpaper/ regex ^\h*([^-:\n]+)\n 1:+i
add-highlighter shared/taskpaper/ regex ^\h*-\h+[^\n]*@done[^\n]* 0:+d
add-highlighter shared/taskpaper/ regex \b(([a-z]+://\S+)|((mailto:)[\w+-]+@\S+)) 0:link

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden taskpaper-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden taskpaper-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        ## If the line above is a project indent with a tab
        try %{ execute-keys -draft Z kx <a-k>^\h*([^:\n]+):<ret> z i<tab> }
        # cleanup trailing white spaces on previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
    }
}

}
