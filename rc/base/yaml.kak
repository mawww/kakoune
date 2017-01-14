# http://yaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ya?ml) %{
    set buffer filetype yaml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code yaml      \
    double_string '"' (?<!\\)(\\\\)*"       '' \
    single_string "'" "'"                   '' \
    comment       '#' '$'                   ''

add-highlighter -group /yaml/double_string fill string
add-highlighter -group /yaml/single_string fill string
add-highlighter -group /yaml/comment       fill comment

add-highlighter -group /yaml/code regex ^(---|\.\.\.)$ 0:meta
add-highlighter -group /yaml/code regex ^(\h*:\w*) 0:keyword
add-highlighter -group /yaml/code regex \b(true|false|null)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden yaml-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden yaml-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : yaml-filter-around-selections <ret> }
        # indent after :
        try %{ exec -draft <space> k x <a-k> :$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group yaml-highlight global WinSetOption filetype=yaml %{ add-highlighter ref yaml }

hook global WinSetOption filetype=yaml %{
    hook window InsertEnd  .* -group yaml-hooks  yaml-filter-around-selections
    hook window InsertChar \n -group yaml-indent yaml-indent-on-new-line
}

hook -group yaml-highlight global WinSetOption filetype=(?!yaml).* %{ remove-highlighter yaml }

hook global WinSetOption filetype=(?!yaml).* %{
    remove-hooks window yaml-indent
    remove-hooks window yaml-hooks
}
