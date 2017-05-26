# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-config %{
    set buffer filetype config
}

hook global BufCreate .*(config|[.]conf) %{
    set buffer filetype config
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / multi_region -default code config \
    string  '"' (?<!\\)(\\\\)*"               '' \
    string  "'" "'"                           '' \
    comment '#' '$'                           ''

addhl -group /config/string  fill string
addhl -group /config/comment fill comment

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _config_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _config_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _config_filter_around_selections <ret> }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=config %{
    addhl ref config

    hook window InsertEnd  .* -group config-hooks  _config_filter_around_selections
    hook window InsertChar \n -group config-indent _config_indent_on_new_line
}

hook global WinSetOption filetype=(?!config).* %{
    rmhl config
    rmhooks window config-indent
    rmhooks window config-hooks
}
