# https://fsharp.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](fs|fsx|fsi) %{
    set-option buffer filetype fsharp
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=fsharp %{
    require-module fsharp

    # indent on newline
    hook window ModeChange pop:insert:.* -group fsharp-trim-indent fsharp-trim-indent
    hook window InsertChar \n -group fsharp-insert fsharp-insert-on-new-line
    hook window InsertChar \n -group fsharp-indent fsharp-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window fsharp-.+ }
}

hook -group fsharp-highlight global WinSetOption filetype=fsharp %{
    add-highlighter window/fsharp ref fsharp
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/fsharp }
}

provide-module fsharp %§

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/fsharp regions
add-highlighter shared/fsharp/code default-region group
add-highlighter shared/fsharp/docstring region \(\*(?!\)) (\*\)) regions
add-highlighter shared/fsharp/double_string region @?(?<!')" (?<!\\)(\\\\)*"B? fill string
add-highlighter shared/fsharp/comment region '//' '$' fill comment
# https://docs.microsoft.com/en-us/dotnet/fsharp/language-reference/attributes 
add-highlighter shared/fsharp/attributes region "\[<" ">\]" fill meta

add-highlighter shared/fsharp/docstring/ default-region fill comment
# ability to write highlighted code inside docstring:
add-highlighter shared/fsharp/docstring/ region '>>> \K' '\z' ref fsharp
add-highlighter shared/fsharp/docstring/ region '\.\.\. \K' '\z' ref fsharp

evaluate-commands %sh{
    # Grammar
    meta="open"

    # exceptions taken from fsharp.vim colors (https://github.com/fsharp/vim-fsharp)
    exceptions="try|failwith|failwithf|finally|invalid_arg|raise|rethrow"

    # keywords taken from fsharp.vim colors (https://github.com/fsharp/vim-fsharp)
    keywords="abstract|as|assert|base|begin|class|default|delegate"
    keywords="${keywords}|do|done|downcast|downto|elif|else|end|exception"
    keywords="${keywords}|extern|for|fun|function|global|if|in|inherit|inline"
    keywords="${keywords}|interface|lazy|let|match|member|module|mutable"
    keywords="${keywords}|namespace|new|of|override|rec|static|struct|then"
    keywords="${keywords}|to|type|upcast|use|val|void|when|while|with"
    keywords="${keywords}|async|atomic|break|checked|component|const|constraint"
    keywords="${keywords}|constructor|continue|decimal|eager|event|external"
    keywords="${keywords}|fixed|functor|include|method|mixin|object|parallel"
    keywords="${keywords}|process|pure|return|seq|tailcall|trait|yield"
    # additional operator keywords (Microsoft.FSharp.Core.Operators)
    keywords="${keywords}|box|hash|sizeof|nameof|typeof|typedefof|unbox|ref|fst|snd"
    keywords="${keywords}|stdin|stdout|stderr"
    # math operators (Microsoft.FSharp.Core.Operators)
    keywords="${keywords}|abs|acos|asin|atan|atan2|ceil|cos|cosh|exp|floor|log"
    keywords="${keywords}|log10|pown|round|sign|sin|sinh|sqrt|tan|tanh"


    types="array|bool|byte|char|decimal|double|enum|exn|float"
    types="${types}|float32|int|int16|int32|int64|lazy_t|list|nativeint"
    types="${types}|obj|option|sbyte|single|string|uint|uint32|uint64"
    types="${types}|uint16|unativeint|unit"

    fsharpCoreMethod="printf|printfn|sprintf|eprintf|eprintfn|fprintf|fprintfn"

    # Add the language's grammar to the static completion list
    printf '%s\n' "hook global WinSetOption filetype=fsharp %{
        set-option window static_words ${values} ${meta} ${exceptions} ${keywords} ${types}
    }" | tr '|' ' '

    # Highlight keywords
    printf '%s\n' "
        add-highlighter shared/fsharp/code/ regex '\b(${meta})\b' 0:meta
        add-highlighter shared/fsharp/code/ regex '\b(${exceptions})\b' 0:function
        add-highlighter shared/fsharp/code/ regex '\b(${fsharpCoreMethod})\b' 0:function
        add-highlighter shared/fsharp/code/ regex '\b(${keywords})\b' 0:keyword
    "
}

# computation expression keywords prefixed with !
add-highlighter shared/fsharp/code/ regex "\w+!" 0:keyword
# brackets
add-highlighter shared/fsharp/code/ regex "[\[\]\(\){}]" 0:bracket
# accomodate typically overloaded operators
add-highlighter shared/fsharp/code/ regex "\B(<<>>|<\|\|>)\B" 0:operator
# fsharp operators
add-highlighter shared/fsharp/code/ regex "\B(->|<-|<=|>=)\B" 0:operator
add-highlighter shared/fsharp/code/ regex "(\b(not)\b|\b(and)\b)" 0:operator
add-highlighter shared/fsharp/code/ regex (?<=[\w\s\d'"_])((\?)([><+-/*%=]{1,2})(\??)|(\??)([><+-/*%=]{1,2})(\?)) 0:operator
add-highlighter shared/fsharp/code/ regex (?<=[\w\s\d'"_])((\|)+>|<(\|)+|<@|@>|<@@|@@>|:>|:\?|:=|(!|#)(?=\w)|:\?>|\?|~([-+]|(~){0,2})) 0:operator
add-highlighter shared/fsharp/code/ regex (?<=[\w\s\d'"_])(<>|::|\h\|\h|(\|\|)+|@|\.\.|<=|>=|(<)+|(>)+|!=|==|\|\|\||(\^)+|(&)+|\+|-|(\*)+|//|/|%+|=) 0:operator
add-highlighter shared/fsharp/code/ regex (?<=[\w\s\d'"_])((?<![=<>!])=(?![=])|[+*-]=) 0:builtin
# integer literals
add-highlighter shared/fsharp/code/ regex %{(?<!\.)\b[1-9]('?\d+)*(u?(l|L|y|s|n)?|[IMm])\b(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?<!\.)\b0b[01]('?[01]+)*(u?(l|L|y|s|n)?|[IMm])\b(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?<!\.)\b0('?[0-7]+)*(u?(l|L|y|s|n)?|[IMm])\b(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?<!\.)\b0x[\da-fA-F]('?[\da-fA-F]+)*(u?(l|L|y|s|n)?)?\b(?!\.)} 0:value
# floating point literals
add-highlighter shared/fsharp/code/ regex %{(?i)(?<!\.)\b\d('?\d+)*\.((lf|[fm])\b|\B)(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?i)(?<!\.)\b\d('?\d+)*\.?e[+-]?\d('?\d+)*(lf|[fm])?\b(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?i)(?<!\.)(\b(\d('?\d+)*)|\B)\.\d('?[\d]+)*(e[+-]?\d('?\d+)*)?(lf|[fm])?\b(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?i)(?<!\.)\b0x[\da-f]('?[\da-f]+)*((lf|[fm])\b|\B)(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?i)(?<!\.)\b0x[\da-f]('?[\da-f]+)*\.?p[+-]?\d('?\d+)*)?(lf|[fm])?\b(?!\.)} 0:value
add-highlighter shared/fsharp/code/ regex %{(?i)(?<!\.)\b0x([\da-f]('?[\da-f]+)*)?\.\d('?[\d]+)*(p[+-]?\d('?\d+)*)?(lf|[fm])?\b(?!\.)} 0:value
# character literals
add-highlighter shared/fsharp/code/char regex %{\B'((\\.)|[^'\\])'(\b(B)|\B)} 0:value
# other literals
add-highlighter shared/fsharp/code/ regex "\b(true|false)\b" 0:value
add-highlighter shared/fsharp/code/ regex "\B(\(\))\B" 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden fsharp-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden fsharp-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*//\h* <ret> y jgh P }
    }
}

define-command -hidden fsharp-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with =
        try %{ execute-keys -draft , k x <a-k> =$ <ret> j <a-gt> }
        # indent after line ending with "do"
        try %{ execute-keys -draft , k x <a-k> \bdo$ <ret> j <a-gt> }
    }
}

§
