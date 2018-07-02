# https://golang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.go %{
    set-option buffer filetype go
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/go regions
add-highlighter shared/go/code default-region group
add-highlighter shared/go/back_string region '`' '`' fill string
add-highlighter shared/go/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/go/single_string region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/go/comment region /\* \*/ fill comment
add-highlighter shared/go/comment_line region '//' $ fill comment

add-highlighter shared/go/code/ regex %{-?([0-9]*\.(?!0[xX]))?\b([0-9]+|0[xX][0-9a-fA-F]+)\.?([eE][+-]?[0-9]+)?i?\b} 0:value

evaluate-commands %sh{
    # Grammar
    keywords="break|default|func|interface|select|case|defer|go|map|struct"
    keywords="${keywords}|chan|else|goto|package|switch|const|fallthrough|if|range|type"
    keywords="${keywords}|continue|for|import|return|var"
    types="bool|byte|chan|complex128|complex64|error|float32|float64|int|int16|int32"
    types="${types}|int64|int8|interface|intptr|map|rune|string|struct|uint|uint16|uint32|uint64|uint8"
    values="false|true|nil|iota"
    functions="append|cap|close|complex|copy|delete|imag|len|make|new|panic|print|println|real|recover"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=go %{
        set-option window static_words ${keywords} ${attributes} ${types} ${values} ${functions}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/go/code/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/go/code/ regex \b(${attributes})\b 0:attribute
        add-highlighter shared/go/code/ regex \b(${types})\b 0:type
        add-highlighter shared/go/code/ regex \b(${values})\b 0:value
        add-highlighter shared/go/code/ regex \b(${functions})\b 0:builtin
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden go-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # copy // comments prefix
        try %{ execute-keys -draft \;<c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o>P<esc> }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft k<a-x> <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ execute-keys -draft \;<a-F>)MB <a-k> \A(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

define-command -hidden go-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden go-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group go-highlight global WinSetOption filetype=go %{ add-highlighter window/go ref go }

hook global WinSetOption filetype=go %{
    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange insert:.* -group go-hooks %{ try %{ execute-keys -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group go-indent go-indent-on-new-line
    hook window InsertChar \{ -group go-indent go-indent-on-opening-curly-brace
    hook window InsertChar \} -group go-indent go-indent-on-closing-curly-brace
}

hook -group go-highlight global WinSetOption filetype=(?!go).* %{ remove-highlighter window/go }

hook global WinSetOption filetype=(?!go).* %{
    remove-hooks window go-hooks
    remove-hooks window go-indent
}
