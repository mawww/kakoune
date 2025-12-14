# https://capnproto.org/

hook global BufCreate .*[.](capnp) %{
    set-option buffer filetype capnp
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=capnp %{
    require-module capnp

    set-option window static_words %opt{capnp_static_words}

    hook window ModeChange pop:insert:.* -group capnp-trim-indent capnp-trim-indent
    hook window InsertChar .* -group capnp-indent capnp-indent-on-char
    hook window InsertChar \n -group capnp-indent capnp-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window capnp-.+ }
}

hook -group capnp-highlight global WinSetOption filetype=capnp %{
    add-highlighter window/capnp ref capnp
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/capnp }
}

provide-module capnp %@

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾
 
add-highlighter shared/capnp regions
add-highlighter shared/capnp/code default-region group

add-highlighter shared/capnp/line_comment region '#' '$' fill comment
add-highlighter shared/capnp/string region '"' (?<!\\)(\\\\)*" fill string

add-highlighter shared/capnp/code/ regex '(?i)\b0b[01]+l?\b' 0:value
add-highlighter shared/capnp/code/ regex '(?i)\b0x[\da-f]+l?\b' 0:value
add-highlighter shared/capnp/code/ regex '(?i)\b0o?[0-7]+l?\b' 0:value
add-highlighter shared/capnp/code/ regex '(?i)\b([1-9]\d*|0)l?\b' 0:value
add-highlighter shared/capnp/code/ regex '\b\d+[eE][+-]?\d+\b' 0:value
add-highlighter shared/capnp/code/ regex '(\b\d+)?\.\d+\b' 0:value
add-highlighter shared/capnp/code/ regex '\b\d+\.' 0:value

evaluate-commands %sh{
    builtin_types="Void Bool Text Data List union group Int8 Int16 Int32 Int64 UInt8 UInt16 UInt32 UInt64 Float32 Float64"
    declarations="struct union enum interface const annotation"
    keywords="using extends import"
    values="true false inf"
    
    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    printf %s\\n "declare-option str-list capnp_static_words $(join "${builtin_types}" ' ') $(join "${declarations}" ' ') $(join "${keywords}" ' ') $(join "${values}" ' ')"

    printf %s\\n "add-highlighter shared/capnp/code/ regex '\b($(join "${builtin_types}" '|'))\b' 0:type"
    printf %s\\n "add-highlighter shared/capnp/code/ regex '\b($(join "${declarations}" '|'))\b' 0:keyword"
    printf %s\\n "add-highlighter shared/capnp/code/ regex '\b($(join "${keywords}" '|'))\b' 0:keyword"
    printf %s\\n "add-highlighter shared/capnp/code/ regex '\b($(join "${values}" '|'))\b' 0:value"
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden capnp-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden capnp-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden capnp-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : capnp-trim-indent <ret> }
        # indent after lines ending with opener token
        try %< execute-keys -draft k x <a-k> [[{]\h*$ <ret> j <a-gt> >
        # deindent closer token(s) when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

@
