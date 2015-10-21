# http://dlang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.di? %{
    set buffer mimetype ""
    set buffer filetype dlang
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code dlang \
    string '"' (?<!\\)(\\\\)*" '' \
    verbatim_string ` ` '' \
    verbatim_string_prefixed 'r"' '"' '' \
    token '#' '\n' '' \
    disabled /\+ \+/ '' \
    comment /\* \*/ '' \
    comment '//' $ ''

addhl -group /dlang/string fill string
addhl -group /dlang/verbatim_string fill magenta
addhl -group /dlang/verbatim_string_prefixed fill magenta
addhl -group /dlang/token fill meta
addhl -group /dlang/disabled fill rgb:777777
addhl -group /dlang/comment fill comment

addhl -group /dlang/string regex %{\\(x[0-9a-fA-F]{2}|[0-7]{1,3}|u[0-9a-fA-F]{4}|U[0-9a-fA-F]{8})\>} 0:value
addhl -group /dlang/code regex %{\<(true|false|null|__FILE__|__MODULE__|__LINE__|__FUNCTION__|__PRETTY_FUNCTION__|__DATE__|__EOF__|__TIME__|__TIMESTAMP__|__VENDOR__|__VERSION__)\>|'((\\.)?|[^'\\])'} 0:value
addhl -group /dlang/code regex "-?([0-9_]*\.(?!0[xXbB]))?\<([0-9_]+|0[xX][0-9a-fA-F_]*\.?[0-9a-fA-F_]+|0[bb][01_]+)([ep]-?[0-9_]+)?[fFlLuUi]*\>" 0:value
addhl -group /dlang/code regex "\<(this)\>\s*[^(]" 1:value
addhl -group /dlang/code regex "\<(bool|byte|cdouble|cfloat|char|creal|dchar|double|dstring|float|idouble|ifloat|int|ireal|long|ptrdiff_t|real|size_t|short|string|ubyte|uint|ulong|ushort|void|wchar|wstring)\>" 0:type
addhl -group /dlang/code regex "\<(alias|asm|assert|body|cast|class|delegate|delete|enum|function|import|in|interface|invariant|is|lazy|mixin|module|new|out|pragma|struct|super|typeid|typeof|union|unittest|__parameters|__traits|__vector)\>" 0:keyword
addhl -group /dlang/code regex "\<(break|case|catch|continue|default|do|else|finally|for|foreach|foreach_reverse|goto|if|return|switch|throw|try|with|while)\>" 0:keyword
addhl -group /dlang/code regex "\<(abstract|align|auto|const|debug|deprecated|export|extern|final|immutable|inout|nothrow|package|private|protected|public|pure|ref|override|scope|shared|static|synchronized|version|__gshared)\>" 0:attribute
addhl -group /dlang/code regex "@(disable|property|nogc|safe|trusted|system)" 0:attribute

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _dlang-indent-on-new-line %~
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

def -hidden _dlang-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _dlang-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

decl str dlang_dfmt_options ""
def dlang-format-dfmt -docstring "Format the code using the dfmt utility" %{
    %sh{
        readonly x=$((kak_cursor_column - 1))
        readonly y="${kak_cursor_line}"

        echo "exec -draft %{%|dfmt<space>${kak_opt_dlang_dfmt_options// /<space>}<ret>}"
        echo "exec gg ${y}g ${x}l"
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=dlang %{
    addhl ref dlang

    # cleanup trailing whitespaces when exiting insert mode
    hook window InsertEnd .* -group dlang-hooks %{ try %{ exec -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group dlang-indent _dlang-indent-on-new-line
    hook window InsertChar \{ -group dlang-indent _dlang-indent-on-opening-curly-brace
    hook window InsertChar \} -group dlang-indent _dlang-indent-on-closing-curly-brace

    alias window format-code dlang-format-dfmt
}

hook global WinSetOption filetype=(?!dlang).* %{
    rmhl dlang

    rmhooks window dlang-hooks
    rmhooks window dlang-indent

    unalias window format-code dlang-format-dfmt
}
