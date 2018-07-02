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
add-highlighter shared/nim/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/nim/triple_string region '"""' '"""' fill string
add-highlighter shared/nim/comment region '#?#\[' '\]##?' fill comment

add-highlighter shared/nim/code/ regex \b(0[xXocCbB])?[\d_]+('[iIuUfFdD](8|16|32|64|128))?\b 0:value
add-highlighter shared/nim/code/ regex \b\d+\.\d+\b 0:value
add-highlighter shared/nim/code/ regex %{'[^'\n]'} 0:string

evaluate-commands %sh{
    # Grammar
    keywords="addr|and|as|asm|atomic|bind|block|break|case|cast|concept|const"
    keywords="${keywords}|continue|converter|defer|discard|distinct|div|do|elif"
    keywords="${keywords}|else|end|enum|except|export|finally|for|from|func"
    keywords="${keywords}|generic|if|import|in|include|interface|is|isnot"
    keywords="${keywords}|iterator|let|macro|method|mixin|mod|nil|not|notin"
    keywords="${keywords}|of|or|out|proc|ptr|raise|ref|return|shl|shr"
    keywords="${keywords}|static|template|try|type|using|var|when|while"
    keywords="${keywords}|with|without|xor|yield"
    types="int|int8|int16|int32|int64|uint|uint8|uint16|uint32|uint64|float"
    types="${types}|float32|float64|bool|char|object|seq|array|cstring|string"
    types="${types}|tuple|varargs"
    values="false|true"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=nim %{
        set-option window static_words ${keywords} ${types} ${values}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/nim/code/ regex \b(${keywords})\b 0:keyword
        add-highlighter shared/nim/code/ regex \b(${types})\b 0:type
        add-highlighter shared/nim/code/ regex \b(${values})\b 0:value
    "
}

add-highlighter shared/nim/code/ regex '#[^\n]+' 0:comment

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
        # indent after line ending with const, let, var, ':' or '='
        try %{ exec -draft <space> k x <a-k> (:|=|const|let|var)$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group nim-highlight global WinSetOption filetype=nim %{ add-highlighter window/nim ref nim }

hook global WinSetOption filetype=nim %{
    hook window InsertChar \n -group nim-indent nim-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange insert:.* -group nim-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook -group nim-highlight global WinSetOption filetype=(?!nim).* %{ remove-highlighter window/nim }

hook global WinSetOption filetype=(?!nim).* %{
    remove-hooks window nim-indent
}
