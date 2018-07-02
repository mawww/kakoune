# http://haml.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](haml) %{
    set-option buffer filetype haml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/haml regions
add-highlighter shared/haml/code    default-region group
add-highlighter shared/haml/comment region ^\h*/          $             fill comment

# Filters
# http://haml.info/docs/yardoc/file.REFERENCE.html#filters
add-highlighter shared/haml/eval1   region -recurse \{ ^\h*%([A-Za-z][A-Za-z0-9_-]*)([#.][A-Za-z][A-Za-z0-9_-]*)?\{\K|#\{\K (?=\}) ref ruby
add-highlighter shared/haml/eval2   region ^\h*[=-]\K     (?<!\|)(?=\n) ref ruby
add-highlighter shared/haml/coffee  region ^\h*:coffee\K  ^\h*[%=-]\K   ref coffee
add-highlighter shared/haml/sass    region ^\h*:sass\K    ^\h*[%=-]\K   ref sass

add-highlighter shared/haml/code/ regex ^\h*(:[a-z]+|-|=)|^(!!!)$ 0:meta
add-highlighter shared/haml/code/ regex ^\h*%([A-Za-z][A-Za-z0-9_-]*)([#.][A-Za-z][A-Za-z0-9_-]*)? 1:keyword 2:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden haml-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden haml-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K/\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : haml-filter-around-selections <ret> }
        # indent after lines beginning with : or -
        try %{ execute-keys -draft k <a-x> <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haml-highlight global WinSetOption filetype=haml %{ add-highlighter window/haml ref haml }

hook global WinSetOption filetype=haml %{
    hook window ModeChange insert:.* -group haml-hooks  haml-filter-around-selections
    hook window InsertChar \n -group haml-indent haml-indent-on-new-line
}

hook -group haml-highlight global WinSetOption filetype=(?!haml).* %{ remove-highlighter window/haml }

hook global WinSetOption filetype=(?!haml).* %{
    remove-hooks window haml-indent
    remove-hooks window haml-hooks
}
