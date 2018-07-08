# http://python.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](py) %{
    set-option buffer filetype python
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/python regions
add-highlighter shared/python/code default-region group
add-highlighter shared/python/docstring     region -match-capture ("""|''') ("""|''') regions
add-highlighter shared/python/double_string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/python/single_string region "'"   (?<!\\)(\\\\)*'  fill string
add-highlighter shared/python/comment       region '#'   '$'              fill comment

# Integer formats
add-highlighter shared/python/code/ regex '(?i)\b0b[01]+l?\b' 0:value
add-highlighter shared/python/code/ regex '(?i)\b0x[\da-f]+l?\b' 0:value
add-highlighter shared/python/code/ regex '(?i)\b0o?[0-7]+l?\b' 0:value
add-highlighter shared/python/code/ regex '(?i)\b([1-9]\d*|0)l?\b' 0:value
# Float formats
add-highlighter shared/python/code/ regex '\b\d+[eE][+-]?\d+\b' 0:value
add-highlighter shared/python/code/ regex '(\b\d+)?\.\d+\b' 0:value
add-highlighter shared/python/code/ regex '\b\d+\.' 0:value
# Imaginary formats
add-highlighter shared/python/code/ regex '\b\d+\+\d+[jJ]\b' 0:value

add-highlighter shared/python/docstring/ default-region fill string
add-highlighter shared/python/docstring/ region '>>> \K'    '\z' ref python
add-highlighter shared/python/docstring/ region '\.\.\. \K'    '\z' ref python

evaluate-commands %sh{
    # Grammar
    values="True|False|None|self|inf"
    meta="import|from"

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

    # built-in exceptions https://docs.python.org/3/library/exceptions.html
    exceptions="ArithmeticError|AssertionError|AttributeError|BaseException|BlockingIOError"
    exceptions="${exceptions}|BrokenPipeError|BufferError|BytesWarning|ChildProcessError"
    exceptions="${exceptions}|ConnectionAbortedError|ConnectionError|ConnectionRefusedError"
    exceptions="${exceptions}|ConnectionResetError|DeprecationWarning|EOFError|Exception"
    exceptions="${exceptions}|FileExistsError|FileNotFoundError|FloatingPointError|FutureWarning"
    exceptions="${exceptions}|GeneratorExit|ImportError|ImportWarning|IndentationError"
    exceptions="${exceptions}|IndexError|InterruptedError|IsADirectoryError|KeyboardInterrupt"
    exceptions="${exceptions}|KeyError|LookupError|MemoryError|ModuleNotFoundError|NameError"
    exceptions="${exceptions}|NotADirectoryError|NotImplementedError|OSError|OverflowError"
    exceptions="${exceptions}|PendingDeprecationWarning|PermissionError|ProcessLookupError"
    exceptions="${exceptions}|RecursionError|ReferenceError|ResourceWarning|RuntimeError"
    exceptions="${exceptions}|RuntimeWarning|StopAsyncIteration|StopIteration|SyntaxError"
    exceptions="${exceptions}|SyntaxWarning|SystemError|SystemExit|TabError|TimeoutError|TypeError"
    exceptions="${exceptions}|UnboundLocalError|UnicodeDecodeError|UnicodeEncodeError|UnicodeError"
    exceptions="${exceptions}|UnicodeTranslateError|UnicodeWarning|UserWarning|ValueError|Warning"
    exceptions="${exceptions}|ZeroDivisionError"

    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="and|as|assert|break|class|continue|def|del|elif|else|except|exec"
    keywords="${keywords}|finally|for|global|if|in|is|lambda|nonlocal|not|or|pass|print"
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
        set-option window static_words ${values} ${meta} ${attributes} ${methods} ${exceptions} ${keywords} ${types} ${functions}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/python/code/ regex '\b(${values})\b' 0:value
        add-highlighter shared/python/code/ regex '\b(${meta})\b' 0:meta
        add-highlighter shared/python/code/ regex '\b(${attribute})\b' 0:attribute
        add-highlighter shared/python/code/ regex '\bdef\s+(${methods})\b' 1:function
        add-highlighter shared/python/code/ regex '\b(${exceptions})\b' 0:function
        add-highlighter shared/python/code/ regex '\b(${keywords})\b' 0:keyword
        add-highlighter shared/python/code/ regex '\b(${functions})\b\(' 1:builtin
        add-highlighter shared/python/code/ regex '\b(${types})\b' 0:type
        add-highlighter shared/python/code/ regex '@[\w_]+\b' 0:attribute
    "
}

add-highlighter shared/python/code/ regex (?<=[\w\s\d'"_])(<=|<<|>>|>=|<>|<|>|!=|==|\||\^|&|\+|-|\*\*|\*|//|/|%|~) 0:operator
add-highlighter shared/python/code/ regex (?<=[\w\s\d'"_])((?<![=<>!])=(?![=])|[+*-]=) 0:builtin

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden python-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*#\h* <ret> y jgh P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k <a-x> s \h+$ <ret> d }
        # indent after line ending with :
        try %{ execute-keys -draft <space> k <a-x> <a-k> :$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group python-highlight global WinSetOption filetype=python %{ add-highlighter window/python ref python }

hook global WinSetOption filetype=python %{
    hook window InsertChar \n -group python-indent python-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange insert:.* -group python-indent %{ try %{ execute-keys -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook -group python-highlight global WinSetOption filetype=(?!python).* %{ remove-highlighter window/python }

hook global WinSetOption filetype=(?!python).* %{
    remove-hooks window python-indent
}
