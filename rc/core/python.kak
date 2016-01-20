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

# Grammar
# ‾‾‾‾‾‾‾

decl -hidden str-list python_values "True:False:None"
decl -hidden str-list python_meta "import:from"
# Keyword list is collected using `keyword.kwlist` from `keyword`
decl -hidden str-list python_keywords "and:as:assert:break:class:continue:def:del:elif:else:except:exec:finally:for:global:if:in:is:lambda:not:or:pass:print:raise:return:try:while:with:yield"
decl -hidden str-list python_types "bool:buffer:bytearray:complex:dict:file:float:frozenset:int:list:long:memoryview:object:set:str:tuple:unicode:xrange"

## FIXME: this variable should be a `flags` type var
# Enumeration that dictates what keywords are statically completed upon
decl str python_static_complete 'values|meta|keywords|types'

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

%sh{
    # Add an highligher for a given group of keywords declared previously
    function addhl_str_list {
        local format=$(printf "$2" "${1//:/|}")
        echo "addhl -group /python/code regex ${format} ${3}:${4}"
    }

    addhl_str_list "$kak_opt_python_values" '\<(%s)\>' 0 value
    addhl_str_list "$kak_opt_python_meta" '\<(%s)\>' 0 meta
    addhl_str_list "$kak_opt_python_keywords" '\<(%s)\>' 0 keyword

    # Highlight types, when they are not used as constructors
    addhl_str_list "$kak_opt_python_types" '\<(%s)\>[^(]' 1 type
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

    %sh{
        # FIXME: the following line tricks kakoune into adding those variables to the environment for later indirection expansion
        # kak_opt_python_values kak_opt_python_meta kak_opt_python_keywords kak_opt_python_types
        for flag in ${kak_opt_python_static_complete//|/ }; do
            keywords_var="kak_opt_python_${flag}"
            keywords=${!keywords_var}

            if [ -n "${keywords}" ]; then
                echo "set -add window static_words '${keywords}'"
            else
                echo "Unsupported flag: ${flag}" >&2
                break
            fi
        done
    }
}

hook global WinSetOption filetype=(?!python).* %{
    rmhl python
    rmhooks window python-indent

    set window static_words ""
}
