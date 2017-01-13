# http://haml.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](haml) %{
    set buffer filetype haml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code haml                                                         \
    comment ^\h*/                                                                 $            '' \
    eval    ^\h*%([A-Za-z][A-Za-z0-9_-]*)([#.][A-Za-z][A-Za-z0-9_-]*)?\{\K|#\{\K (?=\})        \{ \
    eval    ^\h*[=-]\K                                                           (?=[^|]\n)    '' \
    coffee  ^\h*:coffee\K                                                        (?=^\h*[%=-]) '' \
    sass    ^\h*:sass\K                                                          (?=^\h*[%=-]) ''

# Filters
# http://haml.info/docs/yardoc/file.REFERENCE.html#filters

add-highlighter -group /haml/comment fill comment

add-highlighter -group /haml/eval   ref ruby
add-highlighter -group /haml/coffee ref coffee
add-highlighter -group /haml/sass   ref sass

add-highlighter -group /haml/code regex ^\h*(:[a-z]+|-|=)|^(!!!)$ 0:meta
add-highlighter -group /haml/code regex ^\h*%([A-Za-z][A-Za-z0-9_-]*)([#.][A-Za-z][A-Za-z0-9_-]*)? 1:keyword 2:identifier

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden haml-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden haml-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K/\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : haml-filter-around-selections <ret> }
        # indent after lines beginning with : or -
        try %{ exec -draft k <a-x> <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haml-highlight global WinSetOption filetype=haml %{ add-highlighter ref haml }

hook global WinSetOption filetype=haml %{
    hook window InsertEnd  .* -group haml-hooks  haml-filter-around-selections
    hook window InsertChar \n -group haml-indent haml-indent-on-new-line
}

hook -group haml-highlight global WinSetOption filetype=(?!haml).* %{ remove-highlighter haml }

hook global WinSetOption filetype=(?!haml).* %{
    remove-hooks window haml-indent
    remove-hooks window haml-hooks
}
