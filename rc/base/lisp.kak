# http://common-lisp.net
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](lisp) %{
    set buffer filetype lisp
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code lisp \
    string  '"' (?<!\\)(\\\\)*"        '' \
    comment ';' '$'                    ''

add-highlighter -group /lisp/string  fill string
add-highlighter -group /lisp/comment fill comment

add-highlighter -group /lisp/code regex \b(nil|true|false)\b 0:value
add-highlighter -group /lisp/code regex (((\Q***\E)|(///)|(\Q+++\E)){1,3})|(1[+-])|(<|>|<=|=|>=|) 0:operator
add-highlighter -group /lisp/code regex \b(([':]\w+)|([*]\H+[*]))\b 0:identifier
add-highlighter -group /lisp/code regex \b(def[a-z]+|if|do|let|lambda|catch|and|assert|while|def|do|fn|finally|let|loop|new|quote|recur|set!|throw|try|var|case|if-let|if-not|when|when-first|when-let|when-not|(cond(->|->>)?))\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden lisp-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden lisp-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # indent when matches opening paren
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> <a-\;> \; <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group lisp-highlight global WinSetOption filetype=lisp %{ add-highlighter ref lisp }

hook global WinSetOption filetype=lisp %{
    hook window InsertEnd  .* -group lisp-hooks  lisp-filter-around-selections
    hook window InsertChar \n -group lisp-indent lisp-indent-on-new-line
}

hook -group lisp-highlight global WinSetOption filetype=(?!lisp).* %{ remove-highlighter lisp }

hook global WinSetOption filetype=(?!lisp).* %{
    remove-hooks window lisp-indent
    remove-hooks window lisp-hooks
}
