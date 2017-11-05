# http://rust-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](rust|rs) %{
    set-option buffer filetype rust
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code rust \
    string  %{(?<!')"} (?<!\\)(\\\\)*"        '' \
    comment //          $                     '' \
    comment /\*        \*/                   /\*

add-highlighter shared/rust/string  fill string
add-highlighter shared/rust/comment fill comment

add-highlighter shared/rust/code regex \b[A-z0-9_]+! 0:meta
# the number literals syntax is defined here:
# https://doc.rust-lang.org/reference.html#number-literals
add-highlighter shared/rust/code regex \b(?:self|true|false|[0-9][_0-9]*(?:\.[0-9][_0-9]*|(?:\.[0-9][_0-9]*)?E[\+\-][_0-9]+)(?:f(?:32|64))?|(?:0x[_0-9a-fA-F]+|0o[_0-7]+|0b[_01]+|[0-9][_0-9]*)(?:(?:i|u)(?:8|16|32|64|size))?)\b 0:value
add-highlighter shared/rust/code regex \b(?:&&|\|\|)\b 0:operator
# the language keywords are defined here, but many of them are reserved and unused yet:
# https://doc.rust-lang.org/grammar.html#keywords
add-highlighter shared/rust/code regex \b(?:crate|use|extern)\b 0:meta
add-highlighter shared/rust/code regex \b(?:let|as|fn|return|match|if|else|loop|for|in|while|break|continue|move|box|where|impl|pub|unsafe)\b 0:keyword
add-highlighter shared/rust/code regex \b(?:mod|trait|struct|enum|type|mut|ref|static|const)\b 0:attribute
add-highlighter shared/rust/code regex \b(?:u8|u16|u32|u64|usize|i8|i16|i32|i64|isize|f32|f64|bool|char|str|Self)\b 0:type

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden rust-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden rust-indent-on-new-line %~
    evaluate-commands -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K//[!/]?\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : rust-filter-around-selections <ret> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft k <a-x> <a-k> [{(]\h*$ <ret> j <a-gt> ]
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> & }
    >
~

define-command -hidden rust-indent-on-opening-curly-brace %[
    evaluate-commands -draft -itersel %_
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ execute-keys -draft h <a-F> ) M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
    _
]

define-command -hidden rust-indent-on-closing-curly-brace %[
    evaluate-commands -draft -itersel %_
        # align to opening curly brace when alone on a line
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> h m s \A|.\z <ret> 1<a-&> ]
    _
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group rust-highlight global WinSetOption filetype=rust %{ add-highlighter window ref rust }

hook global WinSetOption filetype=rust %[
    hook window InsertEnd  .* -group rust-hooks  rust-filter-around-selections
    hook window InsertChar \n -group rust-indent rust-indent-on-new-line
    hook window InsertChar \{ -group rust-indent rust-indent-on-opening-curly-brace
    hook window InsertChar \} -group rust-indent rust-indent-on-closing-curly-brace
]

hook -group rust-highlight global WinSetOption filetype=(?!rust).* %{ remove-highlighter window/rust }

hook global WinSetOption filetype=(?!rust).* %{
    remove-hooks window rust-indent
    remove-hooks window rust-hooks
}
