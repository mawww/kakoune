# Note: jade is changing its name to pug (https://github.com/pugjs/pug/issues/2184)
# This appears to be a work in progress -- the pug-lang domain is parked, while
# the jade-lang one is active. This highlighter will recognize .pug and .jade extensions,

# http://jade-lang.com (will be http://pug-lang.com)
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](pug|jade) %{
    set buffer filetype pug
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code pug                 \
    text          ^\h*\|\s     $                      '' \
    text          ^\h*([A-Za-z][A-Za-z0-9_-]*)?(\#[A-Za-z][A-Za-z0-9_-]*)?((?:\.[A-Za-z][A-Za-z0-9_-]*)*)?(?<=\S)\h+\K.* $ '' \
    javascript    ^\h*[-=!]    $                      '' \
    double_string '"'          (?:(?<!\\)(\\\\)*"|$)  '' \
    single_string "'"          (?:(?<!\\)(\\\\)*'|$)  '' \
    comment       //           $                      '' \
    attribute    \(            \)                     \( \
    puglang      ^\h*\b(\block|extends|include|append|prepend|if|unless|else|case|when|default|each|while|mixin)\b $ '' \

# Filters
# ‾‾‾‾‾‾‾

add-highlighter -group /pug/double_string    fill    string
add-highlighter -group /pug/single_string    fill    string
add-highlighter -group /pug/comment          fill    comment
add-highlighter -group /pug/javascript       ref     javascript
add-highlighter -group /pug/attribute        ref     javascript
add-highlighter -group /pug/puglang          ref     javascript
add-highlighter -group /pug/puglang          regex   \b(\block|extends|include|append|prepend|if|unless|else|case|when|default|each|while|mixin|of|in)\b 0:keyword
add-highlighter -group /pug/attribute        regex   [()=]                             0:operator
add-highlighter -group /pug/text             regex   \h*(\|)                           1:meta
add-highlighter -group /pug/code             regex   ^\h*([A-Za-z][A-Za-z0-9_-]*)      1:type
add-highlighter -group /pug/code             regex   (\#[A-Za-z][A-Za-z0-9_-]*)        1:identifier
add-highlighter -group /pug/code             regex   ((?:\.[A-Za-z][A-Za-z0-9_-]*)*)   1:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden pug-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden pug-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : pug-filter-around-selections <ret> }
        # copy '//', '|', '-' or '(!)=' prefix and following whitespace
        try %{ exec -draft k <a-x> s ^\h*\K[/|!=-]{1,2}\h* <ret> y gh j P }
        # indent unless we copied something above
        try %{ exec -draft <a-gt> <space> b s \S <ret> g l <a-lt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group pug-highlight global WinSetOption filetype=pug %{ add-highlighter ref pug }

hook global WinSetOption filetype=pug %{
    hook window InsertEnd  .* -group pug-hooks  pug-filter-around-selections
    hook window InsertChar \n -group pug-indent pug-indent-on-new-line
}

hook -group pug-highlight global WinSetOption filetype=(?!pug).* %{ remove-highlighter pug }

hook global WinSetOption filetype=(?!pug).* %{
    remove-hooks window pug-indent
    remove-hooks window pug-hooks
}
