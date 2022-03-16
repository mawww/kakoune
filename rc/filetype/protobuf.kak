# https://developers.google.com/protocol-buffers/

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.proto$ %{
    set-option buffer filetype protobuf
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=protobuf %[
    require-module protobuf

    set-option window static_words %opt{protobuf_static_words}

    hook window ModeChange pop:insert:.* -group protobuf-trim-indent protobuf-trim-indent
    hook -group protobuf-indent window InsertChar \n protobuf-indent-on-newline
    hook -group protobuf-indent window InsertChar \{ protobuf-indent-on-opening-curly-brace
    hook -group protobuf-indent window InsertChar \} protobuf-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window protobuf-.+ }
]

hook -group protobuf-highlight global WinSetOption filetype=protobuf %{
    add-highlighter window/protobuf ref protobuf
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/protobuf }
}

provide-module protobuf %[

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
    keywords='default deprecated enum extend import message oneof option
              package service syntax'
    attributes='optional repeated required'
    types='double float int32 int64 uint32 uint64 sint32 sint64 fixed32 fixed64
           sfixed32 sfixed64 bool string bytes rpc'
    values='false true'

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammer to the static completion list
    printf %s\\n "declare-option str-list protobuf_static_words $(join "${keywords} ${attributes} ${types} ${values}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/protobuf/code/keywords regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/protobuf/code/attributes regex \b($(join "${attributes}" '|'))\b 0:attribute
        add-highlighter shared/protobuf/code/types regex \b($(join "${types}" '|'))\b 0:type
        add-highlighter shared/protobuf/code/values regex \b($(join "${values}" '|'))\b 0:value
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden protobuf-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden protobuf-indent-on-newline %~
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # indent after lines ending with {
        try %[ execute-keys -draft kx <a-k> \{\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        # copy // comments prefix
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}(\h*(?=\S))? <ret> y<c-o>P<esc> }
        # deindent closing brace(s) when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> ]
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

]
