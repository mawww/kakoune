# http://common-lisp.net
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-lisp %{
    set buffer filetype lisp
}

hook global BufCreate .*[.](lisp) %{
    set buffer filetype lisp
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code lisp \
    string  '"' (?<!\\)(\\\\)*"        '' \
    comment ';' '$'                    ''

addhl -group /lisp/string  fill string
addhl -group /lisp/comment fill comment

addhl -group /lisp/code regex \b(nil|true|false)\b 0:value
addhl -group /lisp/code regex (((\Q***\E)|(///)|(\Q+++\E)){1,3})|(1[+-])|(<|>|<=|=|>=|) 0:operator
addhl -group /lisp/code regex \b(([':]\w+)|([*]\H+[*]))\b 0:identifier
addhl -group /lisp/code regex \b(def[a-z]+|if|do|let|lambda|catch|and|assert|while|def|do|fn|finally|let|loop|new|quote|recur|set!|throw|try|var|case|if-let|if-not|when|when-first|when-let|when-not|(cond(->|->>)?))\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _lisp_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _lisp_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # indent when matches opening paren
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> <a-\;> \; <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=lisp %{
    addhl ref lisp

    hook window InsertEnd  .* -group lisp-hooks  _lisp_filter_around_selections
    hook window InsertChar \n -group lisp-indent _lisp_indent_on_new_line
}

hook global WinSetOption filetype=(?!lisp).* %{
    rmhl lisp
    rmhooks window lisp-indent
    rmhooks window lisp-hooks
}
