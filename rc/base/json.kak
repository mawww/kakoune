# http://json.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](json) %{
    set-option buffer filetype json
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/json regions
add-highlighter shared/json/code default-region group
add-highlighter shared/json/string region '"' (?<!\\)(\\\\)*" fill string

add-highlighter shared/json/code/ regex \b(true|false|null|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden json-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden json-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \A|.\z <ret> 1<a-&> >
    >
>

define-command -hidden json-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : json-filter-around-selections <ret> }
        # indent after lines beginning with opener token
        try %< execute-keys -draft k <a-x> <a-k> ^\h*[[{] <ret> j <a-gt> >
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group json-highlight global WinSetOption filetype=json %{ add-highlighter window/json ref json }

hook global WinSetOption filetype=json %{
    hook window ModeChange insert:.* -group json-hooks  json-filter-around-selections
    hook window InsertChar .* -group json-indent json-indent-on-char
    hook window InsertChar \n -group json-indent json-indent-on-new-line
}

hook -group json-highlight global WinSetOption filetype=(?!json).* %{ remove-highlighter window/json }

hook global WinSetOption filetype=(?!json).* %{
    remove-hooks window json-indent
    remove-hooks window json-hooks
}
