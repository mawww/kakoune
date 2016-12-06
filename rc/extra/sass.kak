# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](sass) %{
    set buffer filetype sass
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code sass \
    string  '"' (?<!\\)(\\\\)*"        '' \
    string  "'" "'"                    '' \
    comment '/' '$'                    ''

addhl -group /sass/string  fill string
addhl -group /sass/comment fill comment

addhl -group /sass/code regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:identifier
addhl -group /sass/code regex &|@[A-Za-z][A-Za-z0-9_-]* 0:meta
addhl -group /sass/code regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(em|px)) 0:value
addhl -group /sass/code regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
addhl -group /sass/code regex :(before|after) 0:attribute
addhl -group /sass/code regex !important 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _sass_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _sass_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _sass_filter_around_selections <ret> }
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K/\h* <ret> y j p }
        # avoid indent after properties and comments
        try %{ exec -draft k x <a-K> [:/] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group sass-highlight global WinSetOption filetype=sass %{ addhl ref sass }

hook global WinSetOption filetype=sass %{
    hook window InsertEnd  .* -group sass-hooks  _sass_filter_around_selections
    hook window InsertChar \n -group sass-indent _sass_indent_on_new_line
}

hook -group sass-highlight global WinSetOption filetype=(?!sass).* %{ rmhl sass }

hook global WinSetOption filetype=(?!sass).* %{
    rmhooks window sass-indent
    rmhooks window sass-hooks
}
