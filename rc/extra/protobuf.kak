# https://developers.google.com/protocol-buffers/

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.proto$ %{
    set-option buffer filetype protobuf
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/protobuf regions
add-highlighter shared/protobuf/code default-region group
add-highlighter shared/protobuf/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/protobuf/comment region /\* \*/ fill comment
add-highlighter shared/protobuf/comment_line region '//' $ fill comment

add-highlighter shared/protobuf/code/ regex %{(0x)?[0-9]+\b} 0:value

evaluate-commands %sh{
    # Grammer
    keywords="default|deprecated|enum|extend|import|message|oneof|option"
    keywords="${keywords}|package|service|syntax"
    attributes="optional|repeated|required"
    types="double|float|int32|int64|uint32|uint64|sint32|sint64|fixed32|fixed64"
    types="${types}|sfixed32|sfixed64|bool|string|bytes|rpc"
    values="false|true"

    # Add the language's grammer to the static completion list
    printf '%s\n' "hook global WinSetOption filetype=protobuf %{
        set-option window static_words ${keywords} ${attributes} ${types} ${values}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/protobuf/code/keywords regex \b(${keywords})\b 0:keyword
        add-highlighter shared/protobuf/code/attributes regex \b(${attributes})\b 0:attribute
        add-highlighter shared/protobuf/code/types regex \b(${types})\b 0:type
        add-highlighter shared/protobuf/code/values regex \b(${values})\b 0:value
    "
}

# Commands
# ‾‾‾‾‾‾‾‾


define-command -hidden protobuf-indent-on-newline %~
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        # indent after lines ending with {
        try %[ execute-keys -draft k<a-x> <a-k> \{\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
        # copy // comments prefix
        try %{ execute-keys -draft \;<c-s>k<a-x> s ^\h*\K/{2,}(\h*(?=\S))? <ret> y<c-o>P<esc> }
    ]
~

define-command -hidden protobuf-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden protobuf-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group protobuf-highlight global WinSetOption filetype=protobuf %{ add-highlighter window/protobuf ref protobuf }

hook global WinSetOption filetype=protobuf %[
    hook -group protobuf-indent window InsertChar \n protobuf-indent-on-newline
    hook -group protobuf-indent window InsertChar \{ protobuf-indent-on-opening-curly-brace
    hook -group protobuf-indent window InsertChar \} protobuf-indent-on-closing-curly-brace
]

hook -group protobuf-highlight global WinSetOption filetype=(?!protobuf).* %{ remove-highlighter window/protobuf }

hook global WinSetOption filetype=(?!protobuf).* %{
    remove-hooks window protobuf-hooks
    remove-hooks window protobuf-indent
}
