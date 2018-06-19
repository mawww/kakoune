# http://dlang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.di? %{
    set-option buffer filetype d
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code d \
    string %{(?<!')(?<!'\\)"} %{(?<!\\)(?:\\\\)*"} "" \
    verbatim_string ` ` '' \
    verbatim_string %{(?<!')(?<!'\\)`} %{(?<!\\)(?:\\\\)*`} "" \
    verbatim_string_prefixed %{r`([^(]*)\(} %{\)([^)]*)`} "" \
    disabled '/\+[^+]?' '\+/' '' \
    comment '/\*[^*]?' '\*/' '' \
    comment '//[^/]?' $ '' \
    docstring '/\+\+' '\+/' '' \
    docstring '/\*\*' '\*/' '' \
    docstring /// $ ''

add-highlighter shared/d/string fill string
add-highlighter shared/d/verbatim_string fill magenta
add-highlighter shared/d/verbatim_string_prefixed fill magenta
add-highlighter shared/d/disabled fill rgb:777777
add-highlighter shared/d/comment fill comment
add-highlighter shared/d/docstring fill blue

add-highlighter shared/d/string regex %{\\(x[0-9a-fA-F]{2}|[0-7]{1,3}|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8})\b} 0:value
add-highlighter shared/d/code regex %{'((\\.)?|[^'\\])'} 0:value
add-highlighter shared/d/code regex "-?([0-9_]*\.(?!0[xXbB]))?\b([0-9_]+|0[xX][0-9a-fA-F_]*\.?[0-9a-fA-F_]+|0[bb][01_]+)([ep]-?[0-9_]+)?[fFlLuUi]*\b" 0:value
add-highlighter shared/d/code regex "\b(this)\b\s*[^(]" 1:value
add-highlighter shared/d/code regex "((?:~|\b)this)\b\s*\(" 1:function
add-highlighter shared/d/code regex '#\s*line\b.*' 0:meta

evaluate-commands %sh{
    # Grammar

    keywords="abstract|alias|align|asm|assert|auto|body|break|case|cast"
    keywords="${keywords}|catch|cent|class|const|continue|debug"
    keywords="${keywords}|default|delegate|delete|deprecated|do|else|enum|export|extern"
    keywords="${keywords}|final|finally|for|foreach|foreach_reverse|function|goto"
    keywords="${keywords}|if|immutable|import|in|inout|interface|invariant"
    keywords="${keywords}|is|lazy|macro|mixin|module|new|nothrow|out|override"
    keywords="${keywords}|package|pragma|private|protected|public|pure|ref|return|scope"
    keywords="${keywords}|shared|static|struct|super|switch|synchronized|template"
    keywords="${keywords}|throw|try|typedef|typeid|typeof|union"
    keywords="${keywords}|unittest|version|volatile|while|with"
    attributes="abstract|align|auto|const|debug|deprecated|export|extern|final"
    attributes="${attributes}|immutable|inout|nothrow|package|private|protected"
    attributes="${attributes}|public|pure|ref|override|scope|shared|static|synchronized|version"
    attributes="${attributes}|__gshared|__traits|__vector|__parameters"
    types="bool|byte|cdouble|cent|cfloat|char|creal|dchar|double|dstring|float"
    types="${types}|idouble|ifloat|int|ireal|long|ptrdiff_t|real|size_t|short"
    types="${types}|string|ubyte|ucent|uint|ulong|ushort|void|wchar|wstring"
    values="true|false|null"
    tokens="__FILE__|__MODULE__|__LINE__|__FUNCTION__"
    tokens="${tokens}|__PRETTY_FUNCTION__|__DATE__|__EOF__|__TIME__"
    tokens="${tokens}|__TIMESTAMP__|__VENDOR__|__VERSION__|#line"
    properties="this|init|sizeof|alignof|mangleof|stringof|infinity|nan|dig|epsilon|mant_dig"
    properties="${properties}|max_10_exp|min_exp|max|min_normal|re|im|classinfo"
    properties="${properties}|length|dup|keys|values|rehash|clear"
    decorators="disable|property|nogc|safe|trusted|system"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=d %{
        set-option window static_words ${keywords} ${attributes} ${types} ${values} ${decorators} ${properties}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/d/code regex \b(${keywords})\b 0:keyword
        add-highlighter shared/d/code regex \b(${attributes})\b 0:attribute
        add-highlighter shared/d/code regex \b(${types})\b 0:type
        add-highlighter shared/d/code regex \b(${values})\b 0:value
        add-highlighter shared/d/code regex @(${decorators})\b 0:attribute
        add-highlighter shared/d/code regex \b(${tokens})\b 0:builtin
        add-highlighter shared/d/code regex \.(${properties})\b 1:builtin
    "
}

add-highlighter shared/d/code regex "\bimport\s+([\w._-]+)(?:\s*=\s*([\w._-]+))?" 1:module 2:module
add-highlighter shared/d/code regex "\bmodule\s+([\w_-]+)\b" 1:module

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden d-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # copy // comments prefix
        try %{ execute-keys -draft \;<c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o>P<esc> }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft k<a-x> <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ execute-keys -draft \;<a-F>)MB <a-k> \A(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

define-command -hidden d-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden d-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group d-highlight global WinSetOption filetype=d %{ add-highlighter window ref d }

hook global WinSetOption filetype=d %{
    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange insert:.* -group d-hooks %{ try %{ execute-keys -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group d-indent d-indent-on-new-line
    hook window InsertChar \{ -group d-indent d-indent-on-opening-curly-brace
    hook window InsertChar \} -group d-indent d-indent-on-closing-curly-brace
}

hook -group d-highlight global WinSetOption filetype=(?!d).* %{ remove-highlighter window/d }

hook global WinSetOption filetype=(?!d).* %{
    remove-hooks window d-hooks
    remove-hooks window d-indent
}
