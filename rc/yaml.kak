# http://yaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-yaml %{
    set buffer filetype yaml
}

hook global BufCreate .*[.](yaml) %{
    set buffer filetype yaml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code yaml      \
    double_string '"' (?<!\\)(\\\\)*"       '' \
    single_string "'" "'"                   '' \
    comment       '#' '$'                   ''

addhl -group /yaml/double_string fill string
addhl -group /yaml/single_string fill string
addhl -group /yaml/comment       fill comment

addhl -group /yaml/code regex ^(---|\.\.\.)$ 0:meta
addhl -group /yaml/code regex ^(\h*:\w*) 0:keyword
addhl -group /yaml/code regex \<(true|false|null)\> 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _yaml_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _yaml_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _yaml_filter_around_selections <ret> }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
        # indent after :
        try %{ exec -draft <space> k x <a-k> :$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=yaml %{
    addhl ref yaml

    hook window InsertEnd  .* -group yaml-hooks  _yaml_filter_around_selections
    hook window InsertChar \n -group yaml-indent _yaml_indent_on_new_line
}

hook global WinSetOption filetype=(?!yaml).* %{
    rmhl yaml
    rmhooks window yaml-indent
    rmhooks window yaml-hooks
}
