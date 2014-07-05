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

addhl -group / multi_region -default code json \
    string '"' (?<!\\)(\\\\)*" ''

addhl -group /json/string fill string

addhl -group /json/code regex \<(true|false|null)\> 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _json_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _json_indent_on_char "
    eval -draft -itersel '
        # indent closer token to its opener
        try %_ exec -draft gh <a-k> ^\h*[]}] <ret> m <a-&> _
    '
"

def -hidden _json_indent_on_new_line "
    eval -draft -itersel '
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _json_filter_around_selections <ret> }
        # indent after lines beginning with opener token
        try %_ exec -draft k x <a-k> ^\h*[[{] <ret> j <a-gt> _
    '
"

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=json %{
    addhl ref json

    hook window InsertEnd  .* -group json-hooks  _json_filter_around_selections
    hook window InsertChar .* -group json-indent _json_indent_on_char
    hook window InsertChar \n -group json-indent _json_indent_on_new_line
}

hook global WinSetOption filetype=(?!json).* %{
    rmhl json
    rmhooks window json-indent
    rmhooks window json-hooks
}
