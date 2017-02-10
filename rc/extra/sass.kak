# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](sass) %{
    set buffer filetype sass
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code sass \
    string  '"' (?<!\\)(\\\\)*"        '' \
    string  "'" "'"                    '' \
    comment '/' '$'                    ''

add-highlighter -group /sass/string  fill string
add-highlighter -group /sass/comment fill comment

add-highlighter -group /sass/code regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:variable
add-highlighter -group /sass/code regex &|@[A-Za-z][A-Za-z0-9_-]* 0:meta
add-highlighter -group /sass/code regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(em|px)) 0:value
add-highlighter -group /sass/code regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
add-highlighter -group /sass/code regex :(before|after) 0:attribute
add-highlighter -group /sass/code regex !important 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden sass-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden sass-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K/\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : sass-filter-around-selections <ret> }
        # avoid indent after properties and comments
        try %{ exec -draft k <a-x> <a-K> [:/] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group sass-highlight global WinSetOption filetype=sass %{ add-highlighter ref sass }

hook global WinSetOption filetype=sass %{
    hook window InsertEnd  .* -group sass-hooks  sass-filter-around-selections
    hook window InsertChar \n -group sass-indent sass-indent-on-new-line
}

hook -group sass-highlight global WinSetOption filetype=(?!sass).* %{ remove-highlighter sass }

hook global WinSetOption filetype=(?!sass).* %{
    remove-hooks window sass-indent
    remove-hooks window sass-hooks
}
