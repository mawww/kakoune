# http://haml.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-haml %{
    set buffer filetype haml
}

hook global BufCreate .*[.](haml) %{
    set buffer filetype haml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code haml                                                         \
    comment ^\h*/                                                                 $            '' \
    eval    ^\h*%([A-Za-z][A-Za-z0-9_-]*)([#.][A-Za-z][A-Za-z0-9_-]*)?\{\K|#\{\K (?=\})        \{ \
    eval    ^\h*[=-]\K                                                           (?=[^|]\n)    '' \
    coffee  ^\h*:coffee\K                                                        (?=^\h*[%=-]) '' \
    sass    ^\h*:sass\K                                                          (?=^\h*[%=-]) ''

# Filters
# http://haml.info/docs/yardoc/file.REFERENCE.html#filters

addhl -group /haml/comment fill comment

addhl -group /haml/eval   ref ruby
addhl -group /haml/coffee ref coffee
addhl -group /haml/sass   ref sass

addhl -group /haml/code regex ^\h*(:[a-z]+|-|=)|^(!!!)$ 0:meta
addhl -group /haml/code regex ^\h*%([A-Za-z][A-Za-z0-9_-]*)([#.][A-Za-z][A-Za-z0-9_-]*)? 1:keyword 2:identifier

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _haml_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _haml_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _haml_filter_around_selections <ret> }
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K/\h* <ret> y j p }
        # indent after lines beginning with : or -
        try %{ exec -draft k x <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haml-highlight global WinSetOption filetype=haml %{ addhl ref haml }

hook global WinSetOption filetype=haml %{
    hook window InsertEnd  .* -group haml-hooks  _haml_filter_around_selections
    hook window InsertChar \n -group haml-indent _haml_indent_on_new_line
}

hook global WinSetOption filetype=(?!haml).* %{
    rmhl haml
    rmhooks window haml-indent
    rmhooks window haml-hooks
}
