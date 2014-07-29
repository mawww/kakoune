# http://w3.org/Style/CSS
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-css %{
    set buffer filetype css
}

hook global BufCreate .*[.](css) %{
    set buffer filetype css
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default selector css \
    declaration [{] [}]                   '' \
    comment    /[*] [*]/                  ''

addhl -group /css/comment fill comment

addhl -group /css/declaration regions content \
    string '"' (?<!\\)(\\\\)*"             '' \
    string "'" "'"                         ''

addhl -group /css/declaration/content/string fill string

addhl -group /css/declaration regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(em|px)) 0:value
addhl -group /css/declaration regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
addhl -group /css/declaration regex :(before|after) 0:attribute
addhl -group /css/declaration regex !important 0:keyword

# element#id element.class
# universal selector
addhl -group /css/selector regex         [A-Za-z][A-Za-z0-9_-]* 0:keyword
addhl -group /css/selector regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:identifier

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _css_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _css_indent_on_new_line %[
    eval -draft -itersel %[
        # preserve previous line indent
        try %[ exec -draft <space> K <a-&> ]
        # filter previous line
        try %[ exec -draft k : _css_filter_around_selections <ret> ]
        # indent after lines ending with with {
        try %[ exec -draft k x <a-k> \{$ <ret> j <a-gt> ]
    ]
]

def -hidden _css_indent_on_closing_curly_brace %[
    eval -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ exec -draft <a-h> <a-k> ^\h+\}$ <ret> m s \`|.\' <ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=css %[
    addhl ref css

    hook window InsertEnd  .* -group css-hooks  _css_filter_around_selections
    hook window InsertChar \n -group css-indent _css_indent_on_new_line
    hook window InsertChar \} -group css-indent _css_indent_on_closing_curly_brace
]

hook global WinSetOption filetype=(?!css).* %{
    rmhl css
    rmhooks window css-indent
    rmhooks window css-hooks
}
