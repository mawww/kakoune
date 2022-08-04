# http://python.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](py) %{
    set-option buffer filetype python
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=python %{
    require-module python

    set-option window static_words %opt{python_static_words}

    hook window InsertChar \n -group python-insert python-insert-on-new-line
    hook window InsertChar \n -group python-indent python-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group python-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window python-.+ }
}

hook -group python-highlight global WinSetOption filetype=python %{
    add-highlighter window/python ref python
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/python }
}

provide-module python %§

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/python regions
add-highlighter shared/python/code default-region group
add-highlighter shared/python/docstring     region -match-capture ^\h*("""|''') (?<!\\)(?:\\\\)*("""|''') regions
add-highlighter shared/python/triple_string region -match-capture ("""|''') (?<!\\)(?:\\\\)*("""|''') fill string
add-highlighter shared/python/double_string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/python/single_string region "'"   (?<!\\)(\\\\)*'  fill string
add-highlighter shared/python/documentation region '##'  '$'              fill documentation
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

add-highlighter shared/python/docstring/ default-region fill documentation
add-highlighter shared/python/docstring/ region '(>>>|\.\.\.) \K'    (?=''')|(?=""") ref python

evaluate-commands %sh{
    # Grammar
    values="True False None self inf"
    meta="import from"

    # attributes and methods list based on https://docs.python.org/3/reference/datamodel.html
    attributes="__annotations__ __closure__ __code__ __defaults__ __dict__ __doc__
                __globals__ __kwdefaults__ __module__ __name__ __qualname__"
    methods="__abs__ __add__ __aenter__ __aexit__ __aiter__ __and__ __anext__
             __await__ __bool__ __bytes__ __call__ __complex__ __contains__
             __del__ __delattr__ __delete__ __delitem__ __dir__ __divmod__
             __enter__ __eq__ __exit__ __float__ __floordiv__ __format__
             __ge__ __get__ __getattr__ __getattribute__ __getitem__
             __gt__ __hash__ __iadd__ __iand__ __ifloordiv__ __ilshift__
             __imatmul__ __imod__ __imul__ __index__ __init__
             __init_subclass__ __int__ __invert__ __ior__ __ipow__
             __irshift__ __isub__ __iter__ __itruediv__ __ixor__ __le__
             __len__ __length_hint__ __lshift__ __lt__ __matmul__
             __missing__ __mod__ __mul__ __ne__ __neg__ __new__ __or__
             __pos__ __pow__ __radd__ __rand__ __rdivmod__ __repr__
             __reversed__ __rfloordiv__ __rlshift__ __rmatmul__ __rmod__
             __rmul__ __ror__ __round__ __rpow__ __rrshift__ __rshift__
             __rsub__ __rtruediv__ __rxor__ __set__ __setattr__
             __setitem__ __set_name__ __slots__ __str__ __sub__
             __truediv__ __xor__"

    # built-in exceptions https://docs.python.org/3/library/exceptions.html
    exceptions="ArithmeticError AssertionError AttributeError BaseException BlockingIOError
                BrokenPipeError BufferError BytesWarning ChildProcessError
                ConnectionAbortedError ConnectionError ConnectionRefusedError
                ConnectionResetError DeprecationWarning EOFError Exception
                FileExistsError FileNotFoundError FloatingPointError FutureWarning
                GeneratorExit ImportError ImportWarning IndentationError
                IndexError InterruptedError IsADirectoryError KeyboardInterrupt
                KeyError LookupError MemoryError ModuleNotFoundError NameError
                NotADirectoryError NotImplementedError OSError OverflowError
                PendingDeprecationWarning PermissionError ProcessLookupError
                RecursionError ReferenceError ResourceWarning RuntimeError
                RuntimeWarning StopAsyncIteration StopIteration SyntaxError
                SyntaxWarning SystemError SystemExit TabError TimeoutError TypeError
                UnboundLocalError UnicodeDecodeError UnicodeEncodeError UnicodeError
                UnicodeTranslateError UnicodeWarning UserWarning ValueError Warning
                ZeroDivisionError"

    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="and as assert async await break class continue def del elif else except exec
              finally for global if in is lambda nonlocal not or pass print
              raise return try while with yield"

    # Collected from `keyword.softkwlist`
    soft_keywords="_ case match"

    types="bool buffer bytearray bytes complex dict file float frozenset int
           list long memoryview object set str tuple unicode xrange"

    functions="abs all any ascii bin breakpoint callable chr classmethod compile complex
               delattr dict dir divmod enumerate eval exec filter
               format frozenset getattr globals hasattr hash help
               hex id __import__ input isinstance issubclass iter
               len locals map max memoryview min next oct open ord
               pow print property range repr reversed round
               setattr slice sorted staticmethod sum super type vars zip"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list python_static_words $(join "${values} ${meta} ${attributes} ${methods} ${exceptions} ${keywords} ${types} ${functions}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/python/code/ regex '\b($(join "${values}" '|'))\b' 0:value
        add-highlighter shared/python/code/ regex '\b($(join "${meta}" '|'))\b' 0:meta
        add-highlighter shared/python/code/ regex '\b($(join "${attributes}" '|'))\b' 0:attribute
        add-highlighter shared/python/code/ regex '\bdef\s+($(join "${methods}" '|'))\b' 1:function
        add-highlighter shared/python/code/ regex '\b($(join "${exceptions}" '|'))\b' 0:function
        add-highlighter shared/python/code/ regex '\b($(join "${keywords} ${soft_keywords}" '|'))\b' 0:keyword
        add-highlighter shared/python/code/ regex '\b($(join "${functions}" '|'))\b\(' 1:builtin
        add-highlighter shared/python/code/ regex '\b($(join "${types}" '|'))\b' 0:type
        add-highlighter shared/python/code/ regex '^\h*(@[\w_.]+))' 1:attribute
    "
}

add-highlighter shared/python/code/ regex (?<=[\w\s\d\)\]'"_])(<=|<<|>>|>=|<>?|>|!=|==|\||\^|&|\+|-|\*\*?|//?|%|~) 0:operator
add-highlighter shared/python/code/ regex (?<=[\w\s\d'"_])((?<![=<>!]):?=(?![=])|[+*-]=) 0:builtin
add-highlighter shared/python/code/ regex ^\h*(?:from|import)\h+(\S+) 1:module

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden python-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*#\h* <ret> y jgh P }
    }
}

define-command -hidden python-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with :
        try %{ execute-keys -draft , k x <a-k> :$ <ret> <a-K> ^\h*# <ret> j <a-gt> }
        # deindent closing brace/bracket when after cursor (for arrays and dictionaries)
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

§
