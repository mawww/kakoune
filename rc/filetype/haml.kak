# http://haml.info
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](haml) %{
    set-option buffer filetype haml
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=haml %{
    require-module haml

    hook window ModeChange pop:insert:.* -group haml-trim-indent haml-trim-indent
    hook window InsertChar \n -group haml-insert haml-insert-on-new-line
    hook window InsertChar \n -group haml-indent haml-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window haml-.+ }
}

hook -group haml-highlight global WinSetOption filetype=haml %{
    add-highlighter window/haml ref haml
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/haml }
}


provide-module haml %[
require-module ruby
require-module coffee
require-module sass

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

define-command -hidden haml-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden haml-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K/\h* <ret> y gh j P }
    }
}

define-command -hidden haml-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : haml-trim-indent <ret> }
        # indent after lines beginning with : or -
        try %{ execute-keys -draft k x <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

]
