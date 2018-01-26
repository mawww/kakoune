hook global BufCreate .*\.java %{
    set-option buffer filetype java
}

add-highlighter shared/ regions -default code java \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ ''

add-highlighter shared/java/string fill string
add-highlighter shared/java/comment fill comment

add-highlighter shared/java/code regex %{\b(this|true|false|null)\b} 0:value
add-highlighter shared/java/code regex "\b(void|int|char|unsigned|float|boolean|double)\b" 0:type
add-highlighter shared/java/code regex "\b(while|for|if|else|do|static|switch|case|default|class|interface|enum|goto|break|continue|return|import|try|catch|throw|new|package|extends|implements|instanceof)\b" 0:keyword
add-highlighter shared/java/code regex "\b(final|public|protected|private|abstract|synchronized|native|transient|volatile)\b" 0:attribute

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden java-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        preserve-previous-line-indent
        # indent after lines ending with { or (
        try %[ execute-keys -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # copy // comments prefix
        try %{ execute-keys -draft \;<c-s>k<a-x> s ^\h*\K/{2,} <ret> y<c-o><c-o>P<esc> }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft k<a-x> <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after keywords
        try %[ execute-keys -draft \;<a-F>)MB <a-k> \A(if|else|while|for|try|catch)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-space><a-gt> ]
    =
~

define-command -hidden java-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden java-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
hook global WinSetOption filetype=java %{
    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange insert:.* -group java-hooks %{ try %{ execute-keys -draft <a-x>s^\h+$<ret>d } }
    hook window InsertChar \n -group java-indent java-indent-on-new-line
    hook window InsertChar \{ -group java-indent java-indent-on-opening-curly-brace
    hook window InsertChar \} -group java-indent java-indent-on-closing-curly-brace
}

hook global WinSetOption filetype=(?!java).* %{
    remove-hooks window java-hooks
    remove-hooks window java-indent
}
hook -group java-highlight global WinSetOption filetype=java %{ add-highlighter window ref java }
hook -group java-highlight global WinSetOption filetype=(?!java).* %{ remove-highlighter window/java }
