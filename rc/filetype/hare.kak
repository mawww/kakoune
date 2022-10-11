# detection
hook global BufCreate .*[.]ha %{
    set-option buffer filetype hare
}

# initialisation
hook global WinSetOption filetype=hare %{
    require-module hare
    hook window ModeChange pop:insert:.* -group hare-trim-indent hare-trim-indent
    hook window InsertChar \n -group hare-indent hare-indent-on-new-line
    hook window InsertChar \n -group hare-insert hare-insert-on-new-line
    hook window InsertChar \{ -group hare-indent hare-indent-on-opening-curly-brace
    hook window InsertChar \} -group hare-indent hare-indent-on-closing-curly-brace
}

hook -group hare-highlight global WinSetOption filetype=hare %{
    add-highlighter window/hare ref hare
    hook -once -always window WinSetOption filetype=*. %{ remove-highlighter window/hare }
}

# highlighters
provide-module hare %§
    add-highlighter shared/hare regions
    add-highlighter shared/hare/code default-region group
    add-highlighter shared/hare/comment region // $ fill comment

    add-highlighter shared/hare/rawstring region ` ` group
    add-highlighter shared/hare/rawstring/ fill string

    add-highlighter shared/hare/string region '"' (?<!\\)(\\\\)*" group
    add-highlighter shared/hare/string/ fill string
    add-highlighter shared/hare/string/ regex '(\\0|\\a|\\b|\\f|\\n|\\r|\\t|\\v|\\\\|\\")' 0:meta
    add-highlighter shared/hare/string/ regex "\\'" 0:meta
    add-highlighter shared/hare/string/ regex "(\\x[0-9a-fA-F]{2})" 0:meta
    add-highlighter shared/hare/string/ regex "(\\u[0-9a-fA-F]{4})" 0:meta
    add-highlighter shared/hare/string/ regex "(\\U[0-9a-fA-F]{8})" 0:meta

    add-highlighter shared/hare/rune region "'" (?<!\\)(\\\\)*' group
    add-highlighter shared/hare/rune/ fill string
    add-highlighter shared/hare/rune/ regex "(\\0|\\a|\\b|\\f|\\n|\\r|\\t|\\v|\\\\|\\')" 0:meta
    add-highlighter shared/hare/rune/ regex '\\"' 0:meta
    add-highlighter shared/hare/rune/ regex "(\\x[0-9a-fA-F]{2})" 0:meta
    add-highlighter shared/hare/rune/ regex "(\\u[0-9a-fA-F]{4})" 0:meta
    add-highlighter shared/hare/rune/ regex "(\\U[0-9a-fA-F]{8})" 0:meta

    # imports
    add-highlighter shared/hare/code/ regex "\buse\s.*?(?=;)" 0:module
    add-highlighter shared/hare/code/ regex "\buse\b" 0:meta

    # functions
    add-highlighter shared/hare/code/ regex "\b([0-9a-zA-Z_]*)\h*\(" 1:function

    # attributes
    add-highlighter shared/hare/code/ regex "@(offset|init|fini|test|noreturn|symbol)\b" 0:attribute

    # declarations
    add-highlighter shared/hare/code/ regex "\b(let|export|const)\b" 0:meta
    add-highlighter shared/hare/code/ regex "\b(fn|type|def)\b" 0:keyword

    # builtins
    add-highlighter shared/hare/code/ regex "\b(len|offset|free|alloc|assert|append|abort|delete|insert|vastart|vaarg|vaend)\b" 0:builtin
    add-highlighter shared/hare/code/ regex "\b(as|is)\b" 0:builtin

    # types
    add-highlighter shared/hare/code/ regex "\b(struct|union|enum)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(nullable|null|void)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(u8|u16|u32|u64|uint)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(i8|i16|i32|i64|int)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(size|uintptr|char)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(f32|f64)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(str|rune)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(bool)\b" 0:type
    add-highlighter shared/hare/code/ regex "\b(valist)\b" 0:type

    # literals
    add-highlighter shared/hare/code/ regex "\b(true|false)\b" 0:value
    add-highlighter shared/hare/code/ regex "\b[0-9]+([eE][-+]?[0-9]+)?(z|(i|u)(8|16|32|64)?)?\b" 0:value
    add-highlighter shared/hare/code/ regex "\b[0-9]+([eE][-+]?[0-9]+)?((?=e)|(?=u)|(?=i))" 0:value
    add-highlighter shared/hare/code/ regex "\b0b[0-1]+(z|(i|u)(8|16|32|64)?)?\b" 0:value
    add-highlighter shared/hare/code/ regex "\b0b[0-1]+((?=u)|(?=i))" 0:value
    add-highlighter shared/hare/code/ regex "\b0o[0-7]+(z|(i|u)(8|16|32|64)?)?\b" 0:value
    add-highlighter shared/hare/code/ regex "\b0o[0-7]+((?=u)|(?=i))" 0:value
    add-highlighter shared/hare/code/ regex "\b0x[0-9a-fA-F]+(z|(i|u)(8|16|32|64)?)?\b" 0:value
    add-highlighter shared/hare/code/ regex "\b0x[0-9a-fA-F]+((?=u)|(?=i))" 0:value

    # floats
    add-highlighter shared/hare/code/ regex "\b[0-9]+\.[0-9]+([eE][-+]?[0-9]+)?(f32|f64)?\b" 0:value
    add-highlighter shared/hare/code/ regex "\b[0-9]+\.[0-9]+([eE][-+]?[0-9]+)?((?=e)|(?=f))" 0:value
    add-highlighter shared/hare/code/ regex "\b[0-9]+([eE][-+]?[0-9]+)?(f32|f64)\b" 0:value
    add-highlighter shared/hare/code/ regex "\b[0-9]+([eE][-+]?[0-9]+)?(?=f)" 0:value

    # constants
    add-highlighter shared/hare/code/ regex "\b[0-9A-Z_]*\b" 0:value

    # control flow
    add-highlighter shared/hare/code/ regex "\b(for|if|else|switch|match|return|break|continue|defer|yield|case|static)\b" 0:keyword

    # operators
    add-highlighter shared/hare/code/ regex "(=|\+|-|\*|/|<|>|!|\?|&|\||\.\.(\.)?)" 0:operator

    # commands
    define-command -hidden hare-indent-on-new-line %{ evaluate-commands -draft -itersel %{
        # preserve indentation on new lines
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft kx <a-k> [{(]\h*$ <ret> j i<tab> ]
        # cleanup trailing white spaces on the previous line
        execute-keys -draft k :hare-trim-indent <ret>
        # indent after match/switch's case statements
        try %[ execute-keys -draft kx <a-k> case\h.*=>\h*$ <ret> j<a-gt> ]
        # deindent closing brace(s) when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
    } }

    define-command -hidden hare-insert-on-new-line %{ evaluate-commands -draft -itersel %{
        try %{ evaluate-commands -draft -save-regs '/"' %{
            # copy the comment prefix
            execute-keys -save-regs '' k x s ^\h*\K//\h* <ret> y
            try %{
                # paste the comment prefix
                execute-keys x j x s ^\h* <ret>P
            }
        } }
        try %{
            # remove trailing whitespace on the above line
            execute-keys -draft k :hare-trim-indent <ret>
        }
    } }

    define-command -hidden hare-indent-on-opening-curly-brace %[
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
    ]

    define-command -hidden hare-indent-on-closing-curly-brace %[
        # align to opening curly brace when alone on a line
        try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
    ]

    define-command -hidden hare-trim-indent %{ evaluate-commands -draft -itersel %{
        # remove trailing whitespace
        try %{ execute-keys -draft x s \h+$ <ret> d }
    } }
§
