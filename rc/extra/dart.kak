# https://dartlang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.dart %{
    set-option buffer filetype dart
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/dart regions
add-highlighter shared/dart/code default-region group
add-highlighter shared/dart/back_string region '`' '`' fill string
add-highlighter shared/dart/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/dart/single_string region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/dart/comment region /\* \*/ fill comment
add-highlighter shared/dart/comment_line region '//' $ fill comment

add-highlighter shared/dart/code/ regex %{-?([0-9]*\.(?!0[xX]))?\b([0-9]+|0[xX][0-9a-fA-F]+)\.?([eE][+-]?[0-9]+)?i?\b} 0:value

evaluate-commands %sh{
    # Grammar
    keywords="abstract|do|import|super|as|in|switch|assert|else|interface|async"
    keywords="${keywords}|enum|is|this|export|library|throw|await|external|mixin|break|extends"
    keywords="${keywords}|new|try|case|factory|typedef|catch|operator|class|final|part"
    keywords="${keywords}|const|finally|rethrow|while|continue|for|return|with|covariant"
    keywords="${keywords}|get|set|yield|default|if|static|deferred|implements"
    generator_keywords="async\*|sync\*|yield\*"

    types="void|bool|num|int|double|dynamic|var"
    values="false|true|null"

    annotations="@[a-zA-Z]+"
    functions="(_?[a-z][a-zA-Z0-9]*)(\(|\w+=>)"
    classes="[A-Z][a-zA-Z0-9]*"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=dart %{
        set-option window static_words ${keywords} ${attributes} ${types} ${values}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/dart/code/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/dart/code/ regex \b(${generator_keywords}) 0:keyword
        add-highlighter shared/dart/code/ regex \b(${attributes})\b 0:attribute
        add-highlighter shared/dart/code/ regex \b(${types})\b 0:type
        add-highlighter shared/dart/code/ regex \b(${values})\b 0:value
        add-highlighter shared/dart/code/ regex \b(${functions}) 2:function
        add-highlighter shared/dart/code/ regex \b(${annotations})\b 0:meta
        add-highlighter shared/dart/code/ regex \b(${classes})\b 0:module
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden dart-indent-on-new-line %~
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

define-command -hidden dart-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden dart-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group dart-highlight global WinSetOption filetype=dart %{ add-highlighter window/dart ref dart }

hook global WinSetOption filetype=dart %{
    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange insert:.* -group dart-hooks %{ try %{ execute-keys -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group dart-indent dart-indent-on-new-line
    hook window InsertChar \{ -group dart-indent dart-indent-on-opening-curly-brace
    hook window InsertChar \} -group dart-indent dart-indent-on-closing-curly-brace
}

hook -group dart-highlight global WinSetOption filetype=(?!dart).* %{ remove-highlighter window/dart }

hook global WinSetOption filetype=(?!dart).* %{
    remove-hooks window dart-hooks
    remove-hooks window dart-indent
}
