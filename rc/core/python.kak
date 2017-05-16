# http://python.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](py) %{
    set buffer filetype python
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

declare-option -docstring "when enable more builtin symbols are highlighted for python" \
    bool python_use_extended_syntax yes

add-highlighter -group / regions -default code python \
    double_string '"""' '"""'            '' \
    single_string "'''" "'''"            '' \
    double_string '"'   (?<!\\)(\\\\)*"  '' \
    single_string "'"   (?<!\\)(\\\\)*'  '' \
    comment       '#'   '$'              ''

add-highlighter -group /python/double_string fill string
add-highlighter -group /python/single_string fill string
add-highlighter -group /python/comment       fill comment

%sh{
    # Grammar
    values="True|False|None"
    meta="import|from"
    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="and|as|assert|break|class|continue|def|del|elif|else|except|exec"
    keywords="${keywords}|finally|for|global|if|in|is|lambda|not|or|pass|print"
    keywords="${keywords}|raise|return|try|while|with|yield"
    types="bool|buffer|bytearray|bytes|complex|dict|file|float|frozenset|int"
    types="${types}|list|long|memoryview|object|set|str|tuple|unicode|xrange"
    functions="abs|all|any|ascii|bin|callable|chr|classmethod|compile|complex"
    functions="${functions}|delattr|dict|dir|divmod|enumerate|eval|exec|filter"
    functions="${functions}|format|frozenset|getattr|globals|hasattr|hash|help"
    functions="${functions}|hex|id|__import__|input|isinstance|issubclass|iter"
    functions="${functions}|len|locals|map|max|memoryview|min|next|oct|open|ord"
    functions="${functions}|pow|print|property|range|repr|reversed|round"
    functions="${functions}|setattr|slice|sorted|staticmethod|sum|super|type|vars|zip"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=python %{
        set window static_words '${values}:${meta}:${keywords}:${types}:${functions}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        add-highlighter -group /python/code regex '\b(${values})\b' 1:value
        add-highlighter -group /python/code regex '(?:^|[^.])\b(${meta})\b' 1:meta
        add-highlighter -group /python/code regex '(?:^|[^.])\b(${keywords})\b' 1:keyword
        add-highlighter -group /python/code regex '(?:^|[^.])\b(${functions})\b\(' 1:builtin
    "

    # Highlight types and attributes
    printf %s "
        add-highlighter -group /python/code regex '\b(${types})\b' 1:type
        add-highlighter -group /python/code regex '@[\w_]+\b' 0:attribute
    "
}

# extended syntax

%sh{ if [ "$kak_opt_python_use_extended_syntax" = "true" ]; then
    values="self"
    # attributes and methods list based on https://docs.python.org/3/reference/datamodel.html
    attributes="__annotations__|__closure__|__code__|__defaults__|__dict__|__doc__"
    attributes="${attributes}|__globals__|__kwdefaults__|__module__|__name__|__qualname__"
    methods="__abs__|__add__|__aenter__|__aexit__|__aiter__|__and__|__anext__"
    methods="${methods}|__await__|__bool__|__bytes__|__call__|__complex__|__contains__"
    methods="${methods}|__del__|__delattr__|__delete__|__delitem__|__dir__|__divmod__"
    methods="${methods}|__enter__|__eq__|__exit__|__float__|__floordiv__|__format__"
    methods="${methods}|__ge__|__get__|__getattr__|__getattribute__|__getitem__"
    methods="${methods}|__gt__|__hash__|__iadd__|__iand__|__ifloordiv__|__ilshift__"
    methods="${methods}|__imatmul__|__imod__|__imul__|__index__|__init__"
    methods="${methods}|__init_subclass__|__int__|__invert__|__ior__|__ipow__"
    methods="${methods}|__irshift__|__isub__|__iter__|__itruediv__|__ixor__|__le__"
    methods="${methods}|__len__|__length_hint__|__lshift__|__lt__|__matmul__"
    methods="${methods}|__missing__|__mod__|__mul__|__ne__|__neg__|__new__|__or__"
    methods="${methods}|__pos__|__pow__|__radd__|__rand__|__rdivmod__|__repr__"
    methods="${methods}|__reversed__|__rfloordiv__|__rlshift__|__rmatmul__|__rmod__"
    methods="${methods}|__rmul__|__ror__|__round__|__rpow__|__rrshift__|__rshift__"
    methods="${methods}|__rsub__|__rtruediv__|__rxor__|__set__|__setattr__"
    methods="${methods}|__setitem__|__set_name__|__slots__|__str__|__sub__"
    methods="${methods}|__truediv__|__xor__"

    printf %s "
        add-highlighter -group /python/code regex '\b(${values})\b' 1:variable
        add-highlighter -group /python/code regex '\.(${attributes})[^(]' 1:attribute
        add-highlighter -group /python/code regex '(def\s+|\.)(${methods})\(' 2:function
    "
fi }

# Integer formats
add-highlighter -group /python/code regex '\b0[bB][01]+[lL]?\b' 0:value
add-highlighter -group /python/code regex '\b0[xX][\da-fA-F]+[lL]?\b' 0:value
add-highlighter -group /python/code regex '\b0[oO]?[0-7]+[lL]?\b' 0:value
add-highlighter -group /python/code regex '\b([1-9]\d*|0)[lL]?\b' 0:value
# Float formats
add-highlighter -group /python/code regex '(\h|\v|[({[])(-((\d+\.\d+)|(\d+\.)|(\.\d+)))([eE][+-]?\d+)?(\h|\v|[)}\]])' 2:value
add-highlighter -group /python/code regex '\b\d+[eE][+-]?\d+\b' 0:value
# Imaginary formats
add-highlighter -group /python/code regex '\b\d+[jJ]\b' 0:value
add-highlighter -group /python/code regex '(\h|\v|[({[])(-((\d+\.\d+)|(\d+\.)|(\.\d+)))([eE][+-]?\d+)?[jJ](\h|\v|[)}\]])' 2:value
add-highlighter -group /python/code regex '\b\d+[eE][+-]?\d+[jJ]\b' 0:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden python-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*#\h* <ret> y jgh P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ exec -draft k <a-x> s \h+$ <ret> d }
        # indent after line ending with :
        try %{ exec -draft <space> k x <a-k> :$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group python-highlight global WinSetOption filetype=python %{ add-highlighter ref python }

hook global WinSetOption filetype=python %{
    hook window InsertChar \n -group python-indent python-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window InsertEnd .* -group python-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook -group python-highlight global WinSetOption filetype=(?!python).* %{ remove-highlighter python }

hook global WinSetOption filetype=(?!python).* %{
    remove-hooks window python-indent
}
