# https://cuelang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](cue) %{
    set-option buffer filetype cue
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=cue %[
    require-module cue

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group cue-trim-indent cue-trim-indent
    hook window InsertChar \n -group cue-insert cue-insert-on-new-line
    hook window InsertChar \n -group cue-indent cue-indent-on-new-line
    hook window InsertChar \{ -group cue-indent cue-indent-on-opening-curly-brace
    hook window InsertChar [)}] -group cue-indent cue-indent-on-closing

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window cue-.+ }
]

hook -group cue-highlight global WinSetOption filetype=cue %{
    add-highlighter window/cue ref cue
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/cue }
}

provide-module cue %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

# https://cuelang.org/docs/references/spec/

add-highlighter shared/cue regions
add-highlighter shared/cue/code default-region group
add-highlighter shared/cue/simple_string    region '"'   (?<!\\)(\\\\)*" regions
add-highlighter shared/cue/simple_bytes     region "'"   (?<!\\)(\\\\)*' fill string
add-highlighter shared/cue/multiline_string region '"""' '"""'           ref shared/cue/simple_string
add-highlighter shared/cue/multiline_bytes  region "'''" "'''"           ref shared/cue/simple_bytes
add-highlighter shared/cue/line_comment     region "//"  "$"             fill comment

add-highlighter shared/cue/simple_string/base default-region fill string
add-highlighter shared/cue/simple_string/interpolation region -recurse \( \\\( \) fill meta

evaluate-commands %sh{
    # Grammar
    binary_digit="[01]"
    decimal_digit="[0-9]"
    hex_digit="[0-9a-fA-F]"
    octal_digit="[0-7]"

    decimal_lit="[1-9](_?${decimal_digit})*"
    binary_lit="0b${binary_digit}(_?${binary_digit})*"
    hex_lit="0[xX]${hex_digit}(_?${hex_digit})*"
    octal_lit="0o${octal_digit}(_?${octal_digit})*"

    decimals="${decimal_digit}(_?${decimal_digit})*"
    multiplier="([KMGTP]i?)?"
    exponent="([eE][+-]?${decimals})"
    si_lit="(${decimals}(\.${decimals})?${multiplier}|\.${decimals}${multiplier})"

    int_lit="\b(${decimal_lit}|${si_lit}|${octal_lit}|${binary_lit}|${hex_lit})\b"
    float_lit="\b${decimals}\.(${decimals})?${exponent}?|\b${decimals}${exponent}\b|\.${decimals}${exponent}?\b"

    operator_chars="(\+|-|\*|/|&|&&|\||\|\||=|==|=~|!|!=|!~|<|>|<=|>=)"
    punctuation="(_\|_|:|::|,|\.|\.\.\.|\(|\{|\[|\)|\}|\])"

    function_calls="\w+(?=\()"
    identifier="(?!\d)[\w_$]+"
    reserved="\b__${identifier}"

    preamble="^(package|import)\b"

    functions="len close and or"
    keywords="for in if let"
    operators="div mod quo rem"
    types="
        bool string bytes rune number
        uint uint8 uint16 uint32 uint64 uint128
        int int8 int16 int32 int64 int128
        float float32 float64
    "
    values="null true false"

    join() { sep=$2; set -- $1; IFS="$sep"; echo "$*"; }

    static_words="$(join "package import ${functions} ${keywords} ${operators} ${types} ${values}" ' ')"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list cue_static_words ${static_words}"

    functions="$(join "${functions}" '|')"
    keywords="$(join "${keywords}" '|')"
    operators="$(join "${operators}" '|')"
    types="$(join "${types}" '|')"
    values="$(join "${values}" '|')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/cue/code/ regex ${float_lit} 0:value
        add-highlighter shared/cue/code/ regex ${function_calls} 0:function
        add-highlighter shared/cue/code/ regex ${int_lit} 0:value
        add-highlighter shared/cue/code/ regex ${operator_chars} 0:operator
        add-highlighter shared/cue/code/ regex ${preamble} 0:keyword
        add-highlighter shared/cue/code/ regex ${punctuation} 0:operator
        add-highlighter shared/cue/code/ regex ${reserved} 0:keyword
        add-highlighter shared/cue/code/ regex \b(${functions})\b 0:builtin
        add-highlighter shared/cue/code/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/cue/code/ regex \b(${operators})\b 0:operator
        add-highlighter shared/cue/code/ regex \b(${types})\b 0:type
        add-highlighter shared/cue/code/ regex \b(${values})\b 0:value
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden cue-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden cue-insert-on-new-line %~
    evaluate-commands -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K//[!/]?\h* <ret> y<c-o>P<esc> }
    >
~

define-command -hidden cue-indent-on-new-line %~
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        try %<
            # only if we didn't copy a comment
            execute-keys -draft x <a-K> ^\h*// <ret>
            # indent after lines ending with { or (
            try %[ execute-keys -draft k x <a-k> [{(]\h*$ <ret> j <a-gt> ]
            # indent after lines ending with [{(].+ and move first parameter to own line
            try %< execute-keys -draft [c[({],[)}] <ret> <a-k> \A[({][^\n]+\n[^\n]*\n?\z <ret> L i<ret><esc> <gt> <a-S> <a-&> >
            # deindent closing brace(s) when after cursor
            try %< execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret>  m <a-S> 1<a-&> >
        >
        # filter previous line
        try %{ execute-keys -draft k : cue-trim-indent <ret> }
    >
~

define-command -hidden cue-indent-on-opening-curly-brace %[
    evaluate-commands -draft -itersel %_
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ execute-keys -draft h <a-F> ) M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
    _
]

define-command -hidden cue-indent-on-closing %[
    evaluate-commands -draft -itersel %_
        # align to opening curly brace or paren when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h*[)}]$ <ret> h m <a-S> 1<a-&> >
    _
]

§
