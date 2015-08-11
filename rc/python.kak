# http://python.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-python %{
    set buffer filetype python
}

hook global BufCreate .*[.](py) %{
    set buffer filetype python
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code python \
    double_string '"""' '"""'            '' \
    single_string "'''" "'''"            '' \
    double_string '"'   (?<!\\)(\\\\)*"  '' \
    single_string "'"   (?<!\\)(\\\\)*'  '' \
    comment       '#'   '$'              ''

addhl -group /python/double_string fill string
addhl -group /python/single_string fill string
addhl -group /python/comment       fill comment

addhl -group /python/code regex \<(True|False|None)\> 0:value
addhl -group /python/code regex \<(import|from)\> 0:meta

# Keyword list is collected using `keyword.kwlist` from `keyword`
addhl -group /python/code regex \<(and|as|assert|break|class|continue|def|del|elif|else|except|exec|finally|for|global|if|in|is|lambda|not|or|pass|print|raise|return|try|while|with|yield)\> 0:keyword
# Highlight types, when they are not used as constructors
addhl -group /python/code regex \<(buffer|bytearray|complex|dict|file|float|frozenset|int|list|long|memoryview|object|set|str|tuple|unicode|xrange)\>[^(] 1:type

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _python_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _python_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _python_filter_around_selections <ret> }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
        # indent after :
        try %{ exec -draft <space> k x <a-k> :$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=python %{
    addhl ref python

    hook window InsertEnd  .* -group python-hooks  _python_filter_around_selections
    hook window InsertChar \n -group python-indent _python_indent_on_new_line
}

hook global WinSetOption filetype=(?!python).* %{
    rmhl python
    rmhooks window python-indent
    rmhooks window python-hooks
}
