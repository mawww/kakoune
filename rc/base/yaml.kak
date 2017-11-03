# http://yaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ya?ml) %{
    set-option buffer filetype yaml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code yaml      \
    double_string '"' (?<!\\)(\\\\)*"       '' \
    single_string "'" "'"                   '' \
    comment       '#' '$'                   ''

add-highlighter shared/yaml/double_string fill string
add-highlighter shared/yaml/single_string fill string
add-highlighter shared/yaml/comment       fill comment

add-highlighter shared/yaml/code regex ^(---|\.\.\.)$ 0:meta
add-highlighter shared/yaml/code regex ^(\h*:\w*) 0:keyword
add-highlighter shared/yaml/code regex \b(true|false|null)\b 0:value
add-highlighter shared/yaml/code regex ^\h*-?\h*(\S+): 1:attribute

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden yaml-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden yaml-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : yaml-filter-around-selections <ret> }
        # indent after :
        try %{ execute-keys -draft <space> k x <a-k> :$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group yaml-highlight global WinSetOption filetype=yaml %{ add-highlighter window ref yaml }

hook global WinSetOption filetype=yaml %{
    hook window InsertEnd  .* -group yaml-hooks  yaml-filter-around-selections
    hook window InsertChar \n -group yaml-indent yaml-indent-on-new-line
}

hook -group yaml-highlight global WinSetOption filetype=(?!yaml).* %{ remove-highlighter window/yaml }

hook global WinSetOption filetype=(?!yaml).* %{
    remove-hooks window yaml-indent
    remove-hooks window yaml-hooks
}
