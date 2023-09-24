hook global BufCreate .*\.odin %{
    set-option buffer filetype odin
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=odin %{
    require-module odin

    set-option window static_words %opt{odin_static_words}

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group odin-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
    hook window InsertChar \n -group odin-insert odin-insert-on-new-line
    hook window InsertChar \n -group odin-indent odin-indent-on-new-line
    hook window InsertChar \{ -group odin-indent odin-indent-on-opening-curly-brace
    hook window InsertChar \} -group odin-indent odin-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window odin-.+ }
}

hook -group odin-highlight global WinSetOption filetype=odin %{
    add-highlighter window/odin ref odin
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/odin }
}

provide-module odin %§

add-highlighter shared/odin regions
add-highlighter shared/odin/code default-region group
add-highlighter shared/odin/string region %{(?<!')"} %{(?<!\\)(\\\\)*"} fill string
add-highlighter shared/odin/rawstring region ` ` fill string
add-highlighter shared/odin/character region %{'} %{(?<!\\)'} fill value

add-highlighter shared/odin/comment region -recurse /\* /\* \*/ fill comment
add-highlighter shared/odin/inline_documentation region /// $ fill documentation
add-highlighter shared/odin/line_comment region // $ fill comment

add-highlighter shared/odin/code/ regex "(?<!\w)@\w+\b" 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden odin-insert-on-new-line %[
    # copy // comments prefix and following white spaces
    try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}\h* <ret> y<c-o>P<esc> }
]

define-command -hidden odin-indent-on-new-line %<
	evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft kx <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft kx <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after keywords
        try %[ execute-keys -draft <semicolon><a-F>)MB <a-k> \A(if|else|while|for|try|catch)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-,><a-gt> ]
        # deindent closing brace(s) when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
    =
>

define-command -hidden odin-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden odin-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

evaluate-commands %sh{
    values='false true nil ---'
    types='bool b8 b16 b32 b64
           int  i8 i16 i32 i64 i128
           uint u8 u16 u32 u64 u128 uintptr
           i16le i32le i64le i128le u16le u32le u64le u128le
           i16be i32be i64be i128be u16be u32be u64be u128be
           f16 f32 f64
           f16le f32le f64le
           f16be f32be f64be
           complex32 complex64 complex128
           quaternion64 quaternion128 quaternion256
           rune
           string cstring
           rawptr
           typeid
           any'
    keywords='asm auto_cast bit_set break case cast context continue defer distinct do dynamic else enum
              fallthrough for foreign if import in map not_in or_else or_return package proc return struct
              switch transmute typeid union using when where'
    attributes=''
    # ---------------------------------------------------------------------------------------------- #
    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }
    # ---------------------------------------------------------------------------------------------- #
    add_highlighter() { printf "add-highlighter shared/odin/code/ regex %s %s\n" "$1" "$2"; }
    # ---------------------------------------------------------------------------------------------- #
    add_word_highlighter() {
      while [ $# -gt 0 ]; do
          words=$1 face=$2; shift 2
          regex="\\b($(join "${words}" '|'))\\b"
          add_highlighter "$regex" "1:$face"
      done
    }
    # ---------------------------------------------------------------------------------------------- #
    printf %s\\n "declare-option str-list odin_static_words $(join "${values} ${types} ${keywords} ${attributes} ${modules}" ' ')"
    # ---------------------------------------------------------------------------------------------- #
    add_word_highlighter "$values" "value" "$types" "type" "$keywords" "keyword" "$attributes" "attribute"
    # ---------------------------------------------------------------------------------------------- #
}

§
