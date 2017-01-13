# http://json.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](json) %{
    set buffer filetype json
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code json \
    string '"' (?<!\\)(\\\\)*" ''

add-highlighter -group /json/string fill string

add-highlighter -group /json/code regex \b(true|false|null|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden json-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden json-indent-on-char %<
    eval -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< exec -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \`|.\' <ret> 1<a-&> >
    >
>

def -hidden json-indent-on-new-line %<
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : json-filter-around-selections <ret> }
        # indent after lines beginning with opener token
        try %< exec -draft k <a-x> <a-k> ^\h*[[{] <ret> j <a-gt> >
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group json-highlight global WinSetOption filetype=json %{ add-highlighter ref json }

hook global WinSetOption filetype=json %{
    hook window InsertEnd  .* -group json-hooks  json-filter-around-selections
    hook window InsertChar .* -group json-indent json-indent-on-char
    hook window InsertChar \n -group json-indent json-indent-on-new-line
}

hook -group json-highlight global WinSetOption filetype=(?!json).* %{ remove-highlighter json }

hook global WinSetOption filetype=(?!json).* %{
    remove-hooks window json-indent
    remove-hooks window json-hooks
}
