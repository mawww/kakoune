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

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code python \
    double_string '"""' '"""'            '' \
    single_string "'''" "'''"            '' \
    double_string '"'   (?<!\\)(\\\\)*"  '' \
    single_string "'"   (?<!\\)(\\\\)*'  '' \
    comment       '#'   '$'              ''

addhl -group /python/double_string fill string
addhl -group /python/single_string fill string
addhl -group /python/comment       fill comment

%sh{
    # Grammar
    values="True|False|None"
    meta="import|from"
    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="and|as|assert|break|class|continue|def|del|elif|else|except|exec|finally|for|global|if|in|is|lambda|not|or|pass|print|raise|return|try|while|with|yield"
    types="bool|buffer|bytearray|complex|dict|file|float|frozenset|int|list|long|memoryview|object|set|str|tuple|unicode|xrange"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=python %{
        set window static_words '${values}:${meta}:${keywords}:${types}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /python/code regex '\b(${values})\b' 0:value
        addhl -group /python/code regex '\b(${meta})\b' 0:meta
        addhl -group /python/code regex '\b(${keywords})\b' 0:keyword
    "

    # Highlight types, when they are not used as constructors
    printf %s "addhl -group /python/code regex '\b(${types})\b[^(]' 1:type"
}

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _python_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ exec -draft k <a-x> s \h+$ <ret> d }
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
    hook window InsertChar \n -group python-indent _python_indent_on_new_line
    # cleanup trailing whitespaces on current line insert end
    hook window InsertEnd .* -group python-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }

    set window formatcmd "autopep8 -"
}

hook global WinSetOption filetype=(?!python).* %{
    rmhl python
    rmhooks window python-indent
}
