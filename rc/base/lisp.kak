# http://common-lisp.net
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](lisp) %{
    set-option buffer filetype lisp
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/lisp regions
add-highlighter shared/lisp/code default-region group
add-highlighter shared/lisp/string  region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/lisp/comment region ';' '$'             fill comment

add-highlighter shared/lisp/code/ regex \b(nil|true|false)\b 0:value
add-highlighter shared/lisp/code/ regex (((\Q***\E)|(///)|(\Q+++\E)){1,3})|(1[+-])|(<|>|<=|=|>=) 0:operator
add-highlighter shared/lisp/code/ regex \b(def[a-z]+|if|do|let|lambda|catch|and|assert|while|def|do|fn|finally|let|loop|new|quote|recur|set!|throw|try|var|case|if-let|if-not|when|when-first|when-let|when-not|(cond(->|->>)?))\b 0:keyword
add-highlighter shared/lisp/code/ regex (#?(['`:]|,@?))?\b[a-zA-Z][\w!$%&*+./:<=>?@^_~-]* 0:variable
add-highlighter shared/lisp/code/ regex \*[a-zA-Z][\w!$%&*+./:<=>?@^_~-]*\* 0:variable
add-highlighter shared/lisp/code/ regex (\b\d+)?\.\d+([eEsSfFdDlL]\d+)?\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden lisp-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden lisp-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # indent when matches opening paren
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> <a-\;> \; <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group lisp-highlight global WinSetOption filetype=lisp %{ add-highlighter window/lisp ref lisp }

hook global WinSetOption filetype=lisp %{
    hook window ModeChange insert:.* -group lisp-hooks  lisp-filter-around-selections
    hook window InsertChar \n -group lisp-indent lisp-indent-on-new-line
}

hook -group lisp-highlight global WinSetOption filetype=(?!lisp).* %{ remove-highlighter window/lisp }

hook global WinSetOption filetype=(?!lisp).* %{
    remove-hooks window lisp-indent
    remove-hooks window lisp-hooks
}
