# http://rust-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-rust %{
    set buffer filetype rust
}

hook global BufCreate .*[.](rust|rs) %{
    set buffer filetype rust
    set buffer mimetype ''
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code rust \
    string  '"' (?<!\\)(\\\\)*"        '' \
    comment //   $                     '' \
    comment /\* \*/                   /\*

addhl -group /rust/string  fill string
addhl -group /rust/comment fill comment

addhl -group /rust/code regex \b[A-z0-9_]+! 0:meta
# the number literals syntax is defined here:
# https://doc.rust-lang.org/reference.html#number-literals
addhl -group /rust/code regex \b(?:self|true|false|[0-9][_0-9]*(?:\.[0-9][_0-9]*|(?:\.[0-9][_0-9]*)?E[\+\-][_0-9]+)(?:f(?:32|64))?|(?:0x[_0-9a-fA-F]+|0o[_0-7]+|0b[_01]+|[0-9][_0-9]*)(?:(?:i|u)(?:8|16|32|64|size))?)\b 0:value
addhl -group /rust/code regex \b(?:&&|\|\|)\b 0:operator
# the language keywords are defined here, but many of them are reserved and unused yet:
# https://doc.rust-lang.org/grammar.html#keywords
addhl -group /rust/code regex \b(?:crate|use|extern)\b 0:meta
addhl -group /rust/code regex \b(?:let|as|fn|return|match|if|else|loop|for|in|while|break|continue|move|box|where|impl|pub|unsafe)\b 0:keyword
addhl -group /rust/code regex \b(?:mod|trait|struct|enum|type|mut|ref|static|const)\b 0:attribute
addhl -group /rust/code regex \b(?:u8|u16|u32|u64|usize|i8|i16|i32|i64|isize|f32|f64|bool|char|str|Self)\b 0:type

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _rust_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _rust_indent_on_new_line %~
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _rust_filter_around_selections <ret> }
        # copy // comments prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K//\h* <ret> y j p }
        # indent after lines ending with { or (
        try %[ exec -draft k <a-x> <a-k> [{(]\h*$ <ret> j <a-gt> ]
        # align to opening paren of previous line
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> s \`\(\h*.|.\' <ret> & }
    >
~

def -hidden _rust_indent_on_opening_curly_brace %[
    eval -draft -itersel %_
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ exec -draft h <a-F> ) M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
    _
]

def -hidden _rust_indent_on_closing_curly_brace %[
    eval -draft -itersel %_
        # align to opening curly brace when alone on a line
        try %[ exec -draft <a-h> <a-k> ^\h+\}$ <ret> h m s \`|.\' <ret> 1<a-&> ]
    _
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group rust-highlight global WinSetOption filetype=rust %{ addhl ref rust }

hook global WinSetOption filetype=rust %[
    hook window InsertEnd  .* -group rust-hooks  _rust_filter_around_selections
    hook window InsertChar \n -group rust-indent _rust_indent_on_new_line
    hook window InsertChar \{ -group rust-indent _rust_indent_on_opening_curly_brace
    hook window InsertChar \} -group rust-indent _rust_indent_on_closing_curly_brace
]

hook global WinSetOption filetype=(?!rust).* %{
    rmhl rust
    rmhooks window rust-indent
    rmhooks window rust-hooks
}
