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
    values="True:False:None"
    meta="import:from"
    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="and:as:assert:break:class:continue:def:del:elif:else:except:exec:finally:for:global:if:in:is:lambda:not:or:pass:print:raise:return:try:while:with:yield"
    types="bool:buffer:bytearray:complex:dict:file:float:frozenset:int:list:long:memoryview:object:set:str:tuple:unicode:xrange"

    # Add the language's grammar to the static completion list
    echo "hook global WinSetOption filetype=python %{
        set -add window static_words '${values}'
        set -add window static_words '${meta}'
        set -add window static_words '${keywords}'
        set -add window static_words '${types}'
    }"

    # Highlight keywords
    echo "
        addhl -group /python/code regex '\<(${values//:/|})\>' 0:value
        addhl -group /python/code regex '\<(${meta//:/|})\>' 0:meta
        addhl -group /python/code regex '\<(${keywords//:/|})\>' 0:keyword
    "

    # Highlight types, when they are not used as constructors
    echo "addhl -group /python/code regex '\<(${types//:/|})\>[^(]' 1:type"
}

# Commands
# ‾‾‾‾‾‾‾‾

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
    hook window InsertChar \n -group python-indent _python_indent_on_new_line

    set window formatcmd "autopep8 -"
}

hook global WinSetOption filetype=(?!python).* %{
    rmhl python
    rmhooks window python-indent

    set window static_words ""
}
