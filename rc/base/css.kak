# http://w3.org/Style/CSS
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](css) %{
    set-option buffer filetype css
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default selector css \
    declaration [{] [}]                   '' \
    comment    /[*] [*]/                  ''

add-highlighter shared/css/comment fill comment

add-highlighter shared/css/declaration regions content \
    string '"' (?<!\\)(\\\\)*"             '' \
    string "'" "'"                         ''

add-highlighter shared/css/declaration/content/string fill string

# https://developer.mozilla.org/en-US/docs/Web/CSS/length
add-highlighter shared/css/declaration regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(ch|cm|em|ex|mm|pc|pt|px|rem|vh|vmax|vmin|vw)) 0:value

add-highlighter shared/css/declaration regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
add-highlighter shared/css/declaration regex :(before|after) 0:attribute
add-highlighter shared/css/declaration regex !important 0:keyword

# element#id element.class
# universal selector
add-highlighter shared/css/selector regex         [A-Za-z][A-Za-z0-9_-]* 0:keyword
add-highlighter shared/css/selector regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden css-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden css-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %[ execute-keys -draft \; K <a-&> ]
        # filter previous line
        try %[ execute-keys -draft k : css-filter-around-selections <ret> ]
        # indent after lines ending with with {
        try %[ execute-keys -draft k <a-x> <a-k> \{$ <ret> j <a-gt> ]
    ]
]

define-command -hidden css-indent-on-closing-curly-brace %[
    evaluate-commands -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> m s \A|.\z <ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group css-highlight global WinSetOption filetype=css %{ add-highlighter window ref css }

hook global WinSetOption filetype=css %[
    hook window InsertEnd  .* -group css-hooks  css-filter-around-selections
    hook window InsertChar \n -group css-indent css-indent-on-new-line
    hook window InsertChar \} -group css-indent css-indent-on-closing-curly-brace
]

hook -group css-highlight global WinSetOption filetype=(?!css).* %{ remove-highlighter window/css }

hook global WinSetOption filetype=(?!css).* %{
    remove-hooks window css-indent
    remove-hooks window css-hooks
}
