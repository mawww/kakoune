# Note: jade is changing its name to pug (https://github.com/pugjs/pug/issues/2184)
# This appears to be a work in progress -- the pug-lang domain is parked, while
# the jade-lang one is active. This highlighter will recognize .pug and .jade extensions,

# http://jade-lang.com (will be http://pug-lang.com)
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](pug|jade) %{
    set-option buffer filetype pug
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/pug regions
add-highlighter shared/pug/code          default-region group
add-highlighter shared/pug/text          region ^\h*\|\s     $                      regex   \h*(\|) 1:meta
add-highlighter shared/pug/text2         region '^\h*([A-Za-z][A-Za-z0-9_-]*)?(#[A-Za-z][A-Za-z0-9_-]*)?((?:\.[A-Za-z][A-Za-z0-9_-]*)*)?(?<!\t)(?<! )(?<!\n)\h+\K.*' $ regex   \h*(\|) 1:meta
add-highlighter shared/pug/javascript    region ^\h*[-=!]    $                      ref javascript
add-highlighter shared/pug/double_string region '"'          (?:(?<!\\)(\\\\)*"|$)  fill string
add-highlighter shared/pug/single_string region "'"          (?:(?<!\\)(\\\\)*'|$)  fill string
add-highlighter shared/pug/comment       region //           $                      fill comment
add-highlighter shared/pug/attribute     region -recurse \( \(            \)        group
add-highlighter shared/pug/puglang       region ^\h*\b(\block|extends|include|append|prepend|if|unless|else|case|when|default|each|while|mixin)\b $ group

# Filters
# ‾‾‾‾‾‾‾

add-highlighter shared/pug/attribute/       ref     javascript
add-highlighter shared/pug/attribute/       regex   [()=]                             0:operator
add-highlighter shared/pug/puglang/         ref     javascript
add-highlighter shared/pug/puglang/         regex   \b(\block|extends|include|append|prepend|if|unless|else|case|when|default|each|while|mixin|of|in)\b 0:keyword
add-highlighter shared/pug/code/            regex   ^\h*([A-Za-z][A-Za-z0-9_-]*)      1:type
add-highlighter shared/pug/code/            regex   '(#[A-Za-z][A-Za-z0-9_-]*)'       1:variable
add-highlighter shared/pug/code/            regex   ((?:\.[A-Za-z][A-Za-z0-9_-]*)*)   1:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden pug-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden pug-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : pug-filter-around-selections <ret> }
        # copy '//', '|', '-' or '(!)=' prefix and following whitespace
        try %{ execute-keys -draft k <a-x> s ^\h*\K[/|!=-]{1,2}\h* <ret> y gh j P }
        # indent unless we copied something above
        try %{ execute-keys -draft <a-gt> <space> b s \S <ret> g l <a-lt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group pug-highlight global WinSetOption filetype=pug %{ add-highlighter window/pug ref pug }

hook global WinSetOption filetype=pug %{
    hook window ModeChange insert:.* -group pug-hooks  pug-filter-around-selections
    hook window InsertChar \n -group pug-indent pug-indent-on-new-line
}

hook -group pug-highlight global WinSetOption filetype=(?!pug).* %{ remove-highlighter window/pug }

hook global WinSetOption filetype=(?!pug).* %{
    remove-hooks window pug-indent
    remove-hooks window pug-hooks
}
