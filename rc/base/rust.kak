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
    comment /\* \*/                   /\* \
    macro  ^\h*?\K# (?<!\\)\n          ''

addhl -group /rust/string  fill string
addhl -group /rust/comment fill comment
addhl -group /rust/macro   fill meta

addhl -group /rust/code regex \<(self|true|false)\> 0:value
addhl -group /rust/code regex \<(&&|\|\|)\> 0:operator
addhl -group /rust/code regex \<(match|if|else|as|assert|fail|yield|break|box|continue|extern|for|in|if|impl|let|loop|once|priv|pub|return|unsafe|while|use|fn|proc)\> 0:keyword
addhl -group /rust/code regex \<(mod|trait|struct|enum|type|mut|ref|static|const|alignof|be|do|offsetof|pure|sizeof|typeof)\> 0:attribute
addhl -group /rust/code regex \<(int|uint|float|char|bool|u8|u16|u32|u64|f32|f64|i8|i16|i32|i64|str)\> 0:type
addhl -group /rust/code regex \<(Share|Copy|Send|Sized|Add|Sub|Mul|Div|Rem|Neg|Not|BitAnd|BitOr|BitXor|Drop|Shl|Shr|Index|Option|Some|None|Result|Ok|Err|Any|AnyOwnExt|AnyRefExt|AnyMutRefExt|Ascii|AsciiCast|OwnedAsciiCast|AsciiStr|IntoBytes|ToCStr|Char|Clone|Eq|Ord|TotalEq|TotalOrd|Ordering|Equiv|Less|Equal|Greater|Container|Mutable|Map|MutableMap|Set|MutableSet|FromIterator|Extendable|Iterator|DoubleEndedIterator|RandomAccessIterator|CloneableIterator|OrdIterator|MutableDoubleEndedIterator|ExactSize|Num|NumCast|CheckedAdd|CheckedSub|CheckedMul|Signed|Unsigned|Round|Primitive|Int|Float|ToPrimitive|FromPrimitive|GenericPath|Path|PosixPath|WindowsPath|RawPtr|Buffer|Writer|Reader|Seek|Str|StrVector|StrSlice|OwnedStr|IntoMaybeOwned|StrBuf|ToStr|IntoStr|Tuple1|Tuple2|Tuple3|Tuple4|Tuple5|Tuple6|Tuple7|Tuple8|Tuple9|Tuple10|Tuple11|Tuple12|ImmutableEqVector|ImmutableTotalOrdVector|ImmutableCloneableVector|OwnedVector|OwnedCloneableVector|OwnedEqVector|MutableVector|MutableTotalOrdVector|Vector|VectorVector|CloneableVector|ImmutableVector)\> 0:identifier

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
        # indent after visibility specifier
        try %[ exec -draft k <a-x> <a-k> ^\h*(public|private|protected):\h*$ <ret> j <a-gt> ]
        # indent after if|else|while|for
        try %[ exec -draft <space> <a-F> ) M B <a-k> \`(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\' <ret> s \`|.\' <ret> 1<a-&> 1<a-space> <a-gt> ]
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
        # add ; after } if class or struct definition
        try %[ exec -draft h m <space> <a-?> (class|struct) <ret> <a-k> \`(class|struct)[^{}\n]+(\n)?\s*\{\' <ret> <a-space> m a \; <esc> ]
    _
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=rust %[
    addhl ref rust

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
