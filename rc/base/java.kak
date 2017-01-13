hook global BufCreate .*\.java %{
    set buffer filetype java
}

add-highlighter -group / regions -default code java \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ ''

add-highlighter -group /java/string fill string
add-highlighter -group /java/comment fill comment

add-highlighter -group /java/code regex %{\b(this|true|false|null)\b} 0:value
add-highlighter -group /java/code regex "\b(void|int|char|unsigned|float|boolean|double)\b" 0:type
add-highlighter -group /java/code regex "\b(while|for|if|else|do|static|switch|case|default|class|interface|enum|goto|break|continue|return|import|try|catch|throw|new|package|extends|implements|instanceof)\b" 0:keyword
add-highlighter -group /java/code regex "\b(final|public|protected|private|abstract|synchronized|native|transient|volatile)\b" 0:attribute

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden java-indent-on-new-line %~
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
        # indent after keywords
        try %[ exec -draft \;<a-F>)MB <a-k> \`(if|else|while|for|try|catch)\h*\(.*\)\h*\n\h*\n?\' <ret> s \`|.\' <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

def -hidden java-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden java-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=java %{
    # cleanup trailing whitespaces when exiting insert mode
    hook window InsertEnd .* -group java-hooks %{ try %{ exec -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group java-indent java-indent-on-new-line
    hook window InsertChar \{ -group java-indent java-indent-on-opening-curly-brace
    hook window InsertChar \} -group java-indent java-indent-on-closing-curly-brace
}

hook global WinSetOption filetype=(?!java).* %{
    remove-hooks window java-hooks
    remove-hooks window java-indent
}
hook -group java-highlight global WinSetOption filetype=java %{ add-highlighter ref java }
hook -group java-highlight global WinSetOption filetype=(?!java).* %{ remove-highlighter java }
