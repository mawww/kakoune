# http://json.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-json %{
    set buffer filetype json
}

hook global BufCreate .*[.](json) %{
    set buffer filetype json
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code json \
    string '"' (?<!\\)(\\\\)*" ''

addhl -group /json/string fill string

addhl -group /json/code regex \b(true|false|null)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _json_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _json_indent_on_char %<
    eval -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< exec -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \`|.\' <ret> 1<a-&> >
    >
>

def -hidden _json_indent_on_new_line %<
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _json_filter_around_selections <ret> }
        # indent after lines beginning with opener token
        try %< exec -draft k x <a-k> ^\h*[[{] <ret> j <a-gt> >
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group json-highlight global WinSetOption filetype=json %{ addhl ref json }

hook global WinSetOption filetype=json %{
    hook window InsertEnd  .* -group json-hooks  _json_filter_around_selections
    hook window InsertChar .* -group json-indent _json_indent_on_char
    hook window InsertChar \n -group json-indent _json_indent_on_new_line
}

hook -group json-highlight global WinSetOption filetype=(?!json).* %{ rmhl json }

hook global WinSetOption filetype=(?!json).* %{
    rmhooks window json-indent
    rmhooks window json-hooks
}
