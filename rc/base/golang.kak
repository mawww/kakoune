# https://golang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.go %{
    set buffer mimetype ""
    set buffer filetype golang
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code golang \
    back_string '`' '`' '' \
    double_string '"' (?<!\\)(\\\\)*" '' \
    single_string "'" (?<!\\)(\\\\)*' '' \
    comment /\* \*/ '' \
    comment '//' $ ''

addhl -group /golang/back_string fill string
addhl -group /golang/double_string fill string
addhl -group /golang/single_string fill string
addhl -group /golang/comment fill comment

addhl -group /golang/code regex %{-?([0-9]*\.(?!0[xX]))?\b([0-9]+|0[xX][0-9a-fA-F]+)\.?([eE][+-]?[0-9]+)?i?\b} 0:value

%sh{
    # Grammar
    keywords="break|default|defer|else|fallthrough|for|func|go|goto|if|import"
    keywords="${keywords}|interface|make|new|package|range|return|select|case|switch|type|continue"
    attributes="const"
    types="bool|byte|chan|complex128|complex64|float32|float64|int|int16|int32"
    types="${types}|int64|int8|interface|intptr|map|rune|string|struct|uint|uint16|uint32|uint64|uint8"
    values="false|true|nil"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=golang %{
        set window static_words '${keywords}:${attributes}:${types}:${values}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /golang/code regex \b(${keywords})\b 0:keyword
        addhl -group /golang/code regex \b(${attributes})\b 0:attribute
        addhl -group /golang/code regex \b(${types})\b 0:type
        addhl -group /golang/code regex \b(${values})\b 0:value
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _golang-indent-on-new-line %~
    eval -draft -itersel %=
        # preserve previous line indent
        try %{ exec -draft \;K<a-&> }
        # indent after lines ending with { or (
        try %[ exec -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ exec -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> s \`\(\h*.|.\' <ret> '<a-;>' & }
        # copy // comments prefix
        try %{ exec -draft \;<c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o><c-o>P<esc> }
        # indent after a switch's case/default statements
        try %[ exec -draft k<a-x> <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ exec -draft \;<a-F>)MB <a-k> \`(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\' <ret> s \`|.\' <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

def -hidden _golang-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _golang-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=golang %{
    addhl ref golang

    # cleanup trailing whitespaces when exiting insert mode
    hook window InsertEnd .* -group golang-hooks %{ try %{ exec -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group golang-indent _golang-indent-on-new-line
    hook window InsertChar \{ -group golang-indent _golang-indent-on-opening-curly-brace
    hook window InsertChar \} -group golang-indent _golang-indent-on-closing-curly-brace

    set window formatcmd "gofmt"
}

hool -group golang-highlight global WinSetOption filetype=(?!golang).* %{ rmhl golang }

hook global WinSetOption filetype=(?!golang).* %{
    rmhooks window golang-hooks
    rmhooks window golang-indent
}
