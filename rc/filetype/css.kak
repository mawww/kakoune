# http://w3.org/Style/CSS
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](css) %{
    set-option buffer filetype css
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=css %[
    require-module css

    hook window ModeChange pop:insert:.* -group css-trim-indent  css-trim-indent
    hook window InsertChar \n -group css-indent css-indent-on-new-line
    hook window InsertChar \} -group css-indent css-indent-on-closing-curly-brace
    set-option buffer extra_word_chars '_' '-'

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window css-.+ }
]

hook -group css-highlight global WinSetOption filetype=css %{
    add-highlighter window/css ref css
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/css }
}


provide-module css %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/css regions
add-highlighter shared/css/selector default-region group
add-highlighter shared/css/declaration region [{] [}]  regions
add-highlighter shared/css/comment    region /[*] [*]/ fill comment

add-highlighter shared/css/declaration/base default-region group
add-highlighter shared/css/declaration/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/css/declaration/single_string region "'" "'"             fill string
add-highlighter shared/css/declaration/comment region /[*] [*]/ fill comment

# https://developer.mozilla.org/en-US/docs/Web/CSS/length
add-highlighter shared/css/declaration/base/ regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(ch|cm|em|ex|mm|pc|pt|px|rem|vh|vmax|vmin|vw)) 0:value

add-highlighter shared/css/declaration/base/ regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
add-highlighter shared/css/declaration/base/ regex :(before|after) 0:attribute
add-highlighter shared/css/declaration/base/ regex !important 0:keyword

# element#id element.class
# universal selector
add-highlighter shared/css/selector/ regex         [A-Za-z][A-Za-z0-9_-]* 0:keyword
add-highlighter shared/css/selector/ regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden css-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden css-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %[ execute-keys -draft <semicolon> K <a-&> ]
        # filter previous line
        try %[ execute-keys -draft k : css-trim-indent <ret> ]
        # indent after lines ending with with {
        try %[ execute-keys -draft k <a-x> <a-k> \{$ <ret> j <a-gt> ]
        # deindent closing brace when after cursor
        try %[ execute-keys -draft <a-x> <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> ]
    ]
]

define-command -hidden css-indent-on-closing-curly-brace %[
    evaluate-commands -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> m s \A|.\z <ret> 1<a-&> ]
    ]
]

]
