# http://w3.org/Style/CSS
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](css) %{
    set buffer filetype css
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default selector css \
    declaration [{] [}]                   '' \
    comment    /[*] [*]/                  ''

add-highlighter -group /css/comment fill comment

add-highlighter -group /css/declaration regions content \
    string '"' (?<!\\)(\\\\)*"             '' \
    string "'" "'"                         ''

add-highlighter -group /css/declaration/content/string fill string

add-highlighter -group /css/declaration regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(em|px)) 0:value
add-highlighter -group /css/declaration regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
add-highlighter -group /css/declaration regex :(before|after) 0:attribute
add-highlighter -group /css/declaration regex !important 0:keyword

# element#id element.class
# universal selector
add-highlighter -group /css/selector regex         [A-Za-z][A-Za-z0-9_-]* 0:keyword
add-highlighter -group /css/selector regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:variable

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden css-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden css-indent-on-new-line %[
    eval -draft -itersel %[
        # preserve previous line indent
        try %[ exec -draft \; K <a-&> ]
        # filter previous line
        try %[ exec -draft k : css-filter-around-selections <ret> ]
        # indent after lines ending with with {
        try %[ exec -draft k <a-x> <a-k> \{$ <ret> j <a-gt> ]
    ]
]

def -hidden css-indent-on-closing-curly-brace %[
    eval -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ exec -draft <a-h> <a-k> ^\h+\}$ <ret> m s \`|.\' <ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group css-highlight global WinSetOption filetype=css %{ add-highlighter ref css }

hook global WinSetOption filetype=css %[
    hook window InsertEnd  .* -group css-hooks  css-filter-around-selections
    hook window InsertChar \n -group css-indent css-indent-on-new-line
    hook window InsertChar \} -group css-indent css-indent-on-closing-curly-brace
]

hook -group css-highlight global WinSetOption filetype=(?!css).* %{ remove-highlighter css }

hook global WinSetOption filetype=(?!css).* %{
    remove-hooks window css-indent
    remove-hooks window css-hooks
}
