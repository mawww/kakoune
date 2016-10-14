# http://dlang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.di? %{
    set buffer mimetype ""
    set buffer filetype d
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code d \
    string '"' (?<!\\)(\\\\)*" '' \
    verbatim_string ` ` '' \
    verbatim_string_prefixed 'r"' '"' '' \
    token '#' '\n' '' \
    disabled /\+ \+/ '' \
    comment /\* \*/ '' \
    comment '//' $ ''

addhl -group /d/string fill string
addhl -group /d/verbatim_string fill magenta
addhl -group /d/verbatim_string_prefixed fill magenta
addhl -group /d/token fill meta
addhl -group /d/disabled fill rgb:777777
addhl -group /d/comment fill comment

addhl -group /d/string regex %{\\(x[0-9a-fA-F]{2}|[0-7]{1,3}|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8})\b} 0:value
addhl -group /d/code regex %{'((\\.)?|[^'\\])'} 0:value
addhl -group /d/code regex "-?([0-9_]*\.(?!0[xXbB]))?\b([0-9_]+|0[xX][0-9a-fA-F_]*\.?[0-9a-fA-F_]+|0[bb][01_]+)([ep]-?[0-9_]+)?[fFlLuUi]*\b" 0:value
addhl -group /d/code regex "\b(this)\b\s*[^(]" 1:value

%sh{
    # Grammar
    keywords="alias|asm|assert|body|cast|class|delegate|delete|enum|function"
    keywords="${keywords}|import|in|interface|invariant|is|lazy|mixin|module"
    keywords="${keywords}|new|out|pragma|struct|super|typeid|typeof|union"
    keywords="${keywords}|unittest|__parameters|__traits|__vector|break|case"
    keywords="${keywords}|catch|continue|default|do|else|finally|for|foreach"
    keywords="${keywords}|foreach_reverse|goto|if|return|switch|throw|try|with|while"
    attributes="abstract|align|auto|const|debug|deprecated|export|extern|final"
    attributes="${attributes}|immutable|inout|nothrow|package|private|protected"
    attributes="${attributes}|public|pure|ref|override|scope|shared|static|synchronized|version|__gshared"
    types="bool|byte|cdouble|cfloat|char|creal|dchar|double|dstring|float"
    types="${types}|idouble|ifloat|int|ireal|long|ptrdiff_t|real|size_t|short"
    types="${types}|string|ubyte|uint|ulong|ushort|void|wchar|wstring"
    values="true|false|null|__FILE__|__MODULE__|__LINE__|__FUNCTION__"
    values="${values}|__PRETTY_FUNCTION__|__DATE__|__EOF__|__TIME__"
    values="${values}|__TIMESTAMP__|__VENDOR__|__VERSION__"
    decorators="disable|property|nogc|safe|trusted|system"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=d %{
        set window static_words '${keywords}:${attributes}:${types}:${values}:${decorators}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /d/code regex \b(${keywords})\b 0:keyword
        addhl -group /d/code regex \b(${attributes})\b 0:attribute
        addhl -group /d/code regex \b(${types})\b 0:type
        addhl -group /d/code regex \b(${values})\b 0:value
        addhl -group /d/code regex @(${decorators})\b 0:attribute
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _d-indent-on-new-line %~
    eval -draft -itersel %=
        # preserve previous line indent
        try %{ exec -draft \;K<a-&> }
        # indent after lines ending with { or (
        try %[ exec -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ exec -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> s \`\(\h*.|.\' <ret> '<a-;>' & }
        # copy // comments prefix
        try %{ exec -draft \;<c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o><c-o>P<esc> }
        # indent after a switch's case/default statements
        try %[ exec -draft k<a-x> <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ exec -draft \;<a-F>)MB <a-k> \`(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\' <ret> s \`|.\' <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

def -hidden _d-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _d-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group d-highlight global WinSetOption filetype=d %{ addhl ref d }

hook global WinSetOption filetype=d %{
    # cleanup trailing whitespaces when exiting insert mode
    hook window InsertEnd .* -group d-hooks %{ try %{ exec -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group d-indent _d-indent-on-new-line
    hook window InsertChar \{ -group d-indent _d-indent-on-opening-curly-brace
    hook window InsertChar \} -group d-indent _d-indent-on-closing-curly-brace

    set window comment_selection_chars "/+:+/"
}

hook -group d-highlight global WinSetOption filetype=(?!d).* %{ rmhl d }

hook global WinSetOption filetype=(?!d).* %{
    rmhooks window d-hooks
    rmhooks window d-indent
}
