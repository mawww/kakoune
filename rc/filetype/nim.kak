# https://nim-lang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.nim(s|ble)? %{
    set-option buffer filetype nim
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/nim regions
add-highlighter shared/nim/code default-region group
add-highlighter shared/nim/triple_string region '([A-Za-z](_?\w)*)?"""' '"""(?!")' fill string
add-highlighter shared/nim/raw_string region [A-Za-z](_?[A-Za-z])*" (?<!")"(?!") fill string
add-highlighter shared/nim/string region (?<!'\\)"(?!') (?<!\\)(\\\\)*" fill string
add-highlighter shared/nim/comment region '#?#\[' '\]##?' fill comment

evaluate-commands %sh{
    # Grammar
    opchars='[=+-/<>@$~&%|!?^.:\\*]'
    opnocol='[=+-/<>@$~&%|!?^.\\*]'
    suffix="('[iIuU](8|16|32|64)|'[fF](32|64)?|'[dDuU]|[fF])?"
    floatsuffix="('[fF](32|64)?|'[dD]|[fF])?"
    hexdigit='[0-9a-fA-F]'
    octdigit='[0-7]'
    bindigit='[01]'
    hexlit="0[xX]${hexdigit}(_?${hexdigit})*"
    declit="\d(_?\d)*"
    octlit="0o${octdigit}(_?${octdigit})*"
    binlit="0[bB]${bindigit}(_?${bindigit})*"
    intlit="\b(${declit}|${hexlit}|${octlit}|${binlit})${suffix}\b"
    exponent="([eE][+-]?${declit})"
    floatlit="\b${declit}(\.${declit}${exponent}?|${exponent})${floatsuffix}\b"

    keywords="addr as asm bind block break case cast concept const continue
    converter defer discard distinct do elif else end enum except export
    finally for from func if import include interface iterator let macro
    method mixin nil out proc ptr raise ref return static template try type
    unsafeAddr using var when while yield with without atomic generic"
    operators="or xor and is isnot in notin of div mod shl shr not"
    types="int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 float
    float32 float64 bool char object seq array cstring string tuple varargs
    typedesc pointer byte set typed untyped void auto"
    values="false true"

    join() { sep=$2; set -- $1; IFS="$sep"; echo "$*"; }

    static_words="$(join "${keywords} ${types} ${operator} ${values}" ' ')"

    # Add the language's grammar to the static completion list
    printf %s "hook global WinSetOption filetype=nim %{
        set-option window static_words ${static_words}
    }"

    keywords="$(join "${keywords}" '|')"
    operators="$(join "${operators}" '|')"
    types="$(join "${types}" '|')"
    values="$(join "${values}" '|')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/nim/code/ regex ${opchars}+ 0:operator
        add-highlighter shared/nim/code/ regex (?<!${opchars}):{1,2}(?!${opchars}) 0:meta
        add-highlighter shared/nim/code/ regex :${opnocol}${opchars}* 0:operator
        add-highlighter shared/nim/code/ regex (?<!${opchars})(\*)(:)(?!${opchars}) 1:operator 2:meta
        add-highlighter shared/nim/code/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/nim/code/ regex \b(${operators})\b 0:operator
        add-highlighter shared/nim/code/ regex \b(${types})\b 0:type
        add-highlighter shared/nim/code/ regex \b(${values})\b 0:value
        add-highlighter shared/nim/code/ regex ${intlit} 0:value
        add-highlighter shared/nim/code/ regex ${floatlit} 0:value
    "
}

add-highlighter shared/nim/code/ regex '(,|;|`|\(\.?|\.?\)|\[[.:]?|\.?\]|\{\.?|\.?\})' 0:meta
add-highlighter shared/nim/code/ regex '#[^\n]+' 0:comment
add-highlighter shared/nim/code/ regex %{'(\\([rcnlftvabe\\"']|0*[12]?\d?\d|x[0-9a-fA-F]{2})|[^'\n])'} 0:string

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden nim-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*#\h* <ret> y jgh P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ exec -draft k <a-x> s \h+$ <ret> d }
        # indent after line ending with type, import, export, const, let, var, ':' or '='
        try %{ exec -draft <space> k x <a-k> (:|=|const|let|var|import|export|type)$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group nim-highlight global WinSetOption filetype=nim %{
    add-highlighter window/nim ref nim
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/nim }
}

hook global WinSetOption filetype=nim %{
    hook window InsertChar \n -group nim-indent nim-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange insert:.* -group nim-trim-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window nim-.+ }
}
