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
    hook window ModeChange pop:insert:.* -group rust-trim-indent rust-trim-indent
    hook window InsertChar \n -group rust-indent rust-indent-on-new-line
    hook window InsertChar \{ -group rust-indent rust-indent-on-opening-curly-brace
    hook window InsertChar [)}] -group rust-indent rust-indent-on-closing
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window rust-.+ }
]

hook -group rust-highlight global WinSetOption filetype=rust %{
    add-highlighter window/rust ref rust
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/rust }
}

provide-module rust %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/rust regions
add-highlighter shared/rust/code default-region group
add-highlighter shared/rust/string           region %{(?<!')"} (?<!\\)(\\\\)*"              fill string
add-highlighter shared/rust/raw_string       region -match-capture %{(?<!')r(#*)"} %{"(#*)} fill string
add-highlighter shared/rust/comment          region -recurse "/\*" "/\*" "\*/"              fill comment
add-highlighter shared/rust/line_comment     region "//" "$"                                fill comment

add-highlighter shared/rust/macro_attributes region -recurse "\[" "#!?\[" "\]" regions
add-highlighter shared/rust/macro_attributes/ default-region fill meta
add-highlighter shared/rust/macro_attributes/string region %{(?<!')"} (?<!\\)(\\\\)*" fill string
add-highlighter shared/rust/macro_attributes/raw_string region -match-capture %{(?<!')r(#*)"} %{"(#*)} fill string

add-highlighter shared/rust/code/byte_literal         regex "'\\\\?.'" 0:value
add-highlighter shared/rust/code/long_quoted          regex "('\w+)[^']" 1:meta
add-highlighter shared/rust/code/field_or_parameter   regex (_?\w+)(?::)(?!:) 1:variable
add-highlighter shared/rust/code/namespace            regex [a-zA-Z](\w+)?(\h+)?(?=::) 0:module
add-highlighter shared/rust/code/field                regex ((?<!\.\.)(?<=\.))_?[a-zA-Z]\w*\b 0:meta
add-highlighter shared/rust/code/function_call        regex _?[a-zA-Z]\w*\s*(?=\() 0:function
add-highlighter shared/rust/code/user_defined_type    regex \b[A-Z]\w*\b 0:type
add-highlighter shared/rust/code/function_declaration regex (?:fn\h+)(_?\w+)(?:<[^>]+?>)?\( 1:function
add-highlighter shared/rust/code/variable_declaration regex (?:let\h+(?:mut\h+)?)(_?\w+) 1:variable
add-highlighter shared/rust/code/macro                regex \b[A-z0-9_]+! 0:meta
# the number literals syntax is defined here:
# https://doc.rust-lang.org/reference.html#number-literals
add-highlighter shared/rust/code/values regex \b(?:self|true|false|[0-9][_0-9]*(?:\.[0-9][_0-9]*|(?:\.[0-9][_0-9]*)?E[\+\-][_0-9]+)(?:f(?:32|64))?|(?:0x[_0-9a-fA-F]+|0o[_0-7]+|0b[_01]+|[0-9][_0-9]*)(?:(?:i|u|f)(?:8|16|32|64|128|size))?)\b 0:value
add-highlighter shared/rust/code/attributes regex \b(?:trait|struct|enum|type|mut|ref|static|const)\b 0:attribute
# the language keywords are defined here, but many of them are reserved and unused yet:
# https://doc.rust-lang.org/grammar.html#keywords
add-highlighter shared/rust/code/keywords             regex \b(?:let|as|fn|return|match|if|else|loop|for|in|while|break|continue|move|box|where|impl|dyn|pub|unsafe|async|await|mod|crate|use|extern)\b 0:keyword
add-highlighter shared/rust/code/builtin_types        regex \b(?:u8|u16|u32|u64|u128|usize|i8|i16|i32|i64|i128|isize|f32|f64|bool|char|str|Self)\b 0:type
add-highlighter shared/rust/code/return               regex \breturn\b 0:meta

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
        try %{ execute-keys -draft <semicolon> K <a-&> }
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
