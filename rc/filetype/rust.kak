# http://rust-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](rust|rs) %{
    set-option buffer filetype rust
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=rust %[
    require-module rust

    hook window ModeChange insert:.* -group rust-trim-indent  rust-trim-indent
    hook window InsertChar \n -group rust-indent rust-indent-on-new-line
    hook window InsertChar \{ -group rust-indent rust-indent-on-opening-curly-brace
    hook window InsertChar [)}] -group rust-indent rust-indent-on-closing
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window rust-.+ }
]

hook -group rust-highlight global WinSetOption filetype=rust %{
    add-highlighter window/rust ref rust
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/rust }
}

# Configuration
# ‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=rust %[
    set window formatcmd 'rustfmt'
]


provide-module rust %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/rust regions
add-highlighter shared/rust/code default-region group
add-highlighter shared/rust/string       region %{(?<!')"} (?<!\\)(\\\\)*"  fill string
add-highlighter shared/rust/raw_string   region -match-capture %{(?<!')r(#*)"} %{"(#*)}  fill string
add-highlighter shared/rust/comment      region -recurse /\* /\*        \*/ fill comment
add-highlighter shared/rust/line_comment region //          $               fill comment

add-highlighter shared/rust/code/ regex \b[A-z0-9_]+! 0:meta
# the number literals syntax is defined here:
# https://doc.rust-lang.org/reference.html#number-literals
add-highlighter shared/rust/code/ regex \b(?:self|true|false|[0-9][_0-9]*(?:\.[0-9][_0-9]*|(?:\.[0-9][_0-9]*)?E[\+\-][_0-9]+)(?:f(?:32|64))?|(?:0x[_0-9a-fA-F]+|0o[_0-7]+|0b[_01]+|[0-9][_0-9]*)(?:(?:i|u|f)(?:8|16|32|64|128|size))?)\b 0:value
add-highlighter shared/rust/code/ regex \b(?:&&|\|\|)\b 0:operator
# the language keywords are defined here, but many of them are reserved and unused yet:
# https://doc.rust-lang.org/grammar.html#keywords
add-highlighter shared/rust/code/ regex (?:#!?\[.*?\]) 0:meta
add-highlighter shared/rust/code/ regex \b(?:let|as|fn|return|match|if|else|loop|for|in|while|break|continue|move|box|where|impl|dyn|pub|unsafe|async|await)\b 0:keyword
add-highlighter shared/rust/code/ regex \b(?:trait|struct|enum|type|mut|ref|static|const)\b 0:attribute
add-highlighter shared/rust/code/ regex \b(?:u8|u16|u32|u64|u128|usize|i8|i16|i32|i64|i128|isize|f32|f64|bool|char|str|Self)\b 0:type
add-highlighter shared/rust/code/ regex \b(?:mod|crate|use|extern)\b 0:module
add-highlighter shared/rust/code/ regex \$\w+\b 0:variable
add-highlighter shared/rust/code/ regex "'\\\\?.'" 0:value
add-highlighter shared/rust/code/ regex "('\w+)[^']" 1:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden rust-trim-indent %{
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
        try %{ execute-keys -draft k : rust-trim-indent <ret> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft k <a-x> <a-k> [{(]\h*$ <ret> j <a-gt> ]
        # indent after lines ending with [{(].+ and move first parameter to own line
        try %< execute-keys -draft [c[({],[)}] <ret> <a-k> \A[({][^\n]+\n[^\n]*\n?\z <ret> L i<ret><esc> <gt> <a-S> <a-&> >
    >
~

define-command -hidden rust-indent-on-opening-curly-brace %[
    evaluate-commands -draft -itersel %_
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ execute-keys -draft h <a-F> ) M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
    _
]

define-command -hidden rust-indent-on-closing %[
    evaluate-commands -draft -itersel %_
        # align to opening curly brace or paren when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h*[)}]$ <ret> h m <a-S> 1<a-&> >
    _
]

§
