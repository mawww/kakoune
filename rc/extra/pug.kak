# Note: jade is changing its name to pug (https://github.com/pugjs/pug/issues/2184)
# This appears to be a work in progress -- the pug-lang domain is parked, while
# the jade-lang one is active. This highlighter will recognize .pug and .jade extensions,

# http://jade-lang.com (will be http://pug-lang.com)
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-pug %{
    set buffer filetype pug
}

hook global BufCreate .*[.](pug|jade) %{
    set buffer filetype pug
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code pug                 \
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

addhl -group /pug/double_string    fill    string
addhl -group /pug/single_string    fill    string
addhl -group /pug/comment          fill    comment
addhl -group /pug/javascript       ref     javascript
addhl -group /pug/attribute        ref     javascript
addhl -group /pug/puglang          ref     javascript
addhl -group /pug/puglang          regex   \b(\block|extends|include|append|prepend|if|unless|else|case|when|default|each|while|mixin|of|in)\b 0:keyword
addhl -group /pug/attribute        regex   [()=]                             0:operator
addhl -group /pug/text             regex   \h*(\|)                           1:meta
addhl -group /pug/code             regex   ^\h*([A-Za-z][A-Za-z0-9_-]*)      1:type
addhl -group /pug/code             regex   (\#[A-Za-z][A-Za-z0-9_-]*)        1:identifier
addhl -group /pug/code             regex   ((?:\.[A-Za-z][A-Za-z0-9_-]*)*)   1:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _pug_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _pug_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _pug_filter_around_selections <ret> }
        # copy '//', '|', '-' or '(!)=' prefix and following whitespace
        try %{ exec -draft k x s ^\h*\K[/|!=-]{1,2}\h* <ret> y j p }
        # indent unless we copied something above
        try %{ exec -draft <a-gt> <space> b s \S <ret> g l <a-lt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=pug %{
    addhl ref pug

    hook window InsertEnd  .* -group pug-hooks  _pug_filter_around_selections
    hook window InsertChar \n -group pug-indent _pug_indent_on_new_line
}

hook global WinSetOption filetype=(?!pug).* %{
    rmhl pug
    rmhooks window pug-indent
    rmhooks window pug-hooks
}
