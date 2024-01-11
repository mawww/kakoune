# http://ponylang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](pony) %{
    set-option buffer filetype pony
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=pony %{
    require-module pony

    set-option window static_words %opt{pony_static_words}

    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange pop:insert:.* -group pony-trim-indent %{ try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d } }
    hook window InsertChar \n -group pony-indent pony-indent-on-new-line
    hook window InsertChar \n -group pony-insert pony-insert-on-new-line

    set-option buffer extra_word_chars '_' "'"

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window pony-.+ }
}

hook -group pony-highlight global WinSetOption filetype=pony %{
    add-highlighter window/pony ref pony
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter pony }
}

provide-module pony %§

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/pony regions
add-highlighter shared/pony/code default-region group

add-highlighter shared/pony/comment region '/\*' '\*/' fill comment
add-highlighter shared/pony/line_comment region '//' '$' fill comment

# String literals
add-highlighter shared/pony/string region -match-capture '("""|")' '(?<!\\)(?:\\\\)*("""|")' group
add-highlighter shared/pony/string/ fill string
add-highlighter shared/pony/string/ regex '(?:\\a|\\b|\\e|\\f|\\n|\\r|\\t||\\v|\\\\|\\''|\\0|\\x[0-9a-fA-F]{2}|\\u[0-9a-fA-F]{4}|\\u[0-9a-fA-F]{6})' 0:meta

# Character literals
# negative lookbehind to account for variables with prime such as myvar' or myvar''
add-highlighter shared/pony/character region "(?<![a-z0-9'])'" (?<!\\)(\\\\)*' group
add-highlighter shared/pony/character/ fill string
add-highlighter shared/pony/character/ regex '(?:\\a|\\b|\\e|\\f|\\n|\\r|\\t||\\v|\\\\|\\''|\\0|\\x[0-9a-fA-F]{2})' 0:meta

# Operators
add-highlighter shared/pony/code/ regex '=|\+|-|\*|/|%|%%|<<|>>|==|!=|<|<=|>=|>|=>|;' 0:operator
add-highlighter shared/pony/code/ regex '(\+|-|\*|/|%|%%|<<|>>|==|!=|<|<=|>=|>)~' 0:operator
add-highlighter shared/pony/code/ regex '(\+|-|\*|/|%|%%)\?' 0:operator
add-highlighter shared/pony/code/ regex '^\h*|' 0:operator
add-highlighter shared/pony/code/ regex '\b_\b' 0:operator

# Integer literals
add-highlighter shared/pony/code/ regex '\b[0-9](_?[0-9])*\b' 0:value
add-highlighter shared/pony/code/ regex '\b0x[0-9a-fA-F](_?[0-9a-fA-F])*\b' 0:value
add-highlighter shared/pony/code/ regex '\b0b[01](_?[01])*\b' 0:value

# Float literals
add-highlighter shared/pony/code/ regex '\b[0-9]+\.[0-9]+(?:[eE][-+]?[0-9]+)?\b' 0:value
add-highlighter shared/pony/code/ regex '\b[0-9]+[eE][-+]?[0-9]+\b' 0:value

# Type literals
add-highlighter shared/pony/code/ regex '\b_?[A-Z][A-Za-z0-9]*\b' 0:type

# Literal words are highlighted below to also allow for autocompletion by appending
# them to static_words.
evaluate-commands %sh{
    # Grammar
    values="true|false|this"
    meta='use'
    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="as|break|match|continue|else|elseif|try|in|return|end|for|is"
    keywords="${keywords}|recover|consume|error|do|then|if|while|where|with"
    keywords="${keywords}|class|struct|object|actor|interface|trait|primitive"
    keywords="${keywords}|type|var|let|embed|repeat|until|addressof"
    func_decl="new|fun|be"
    capabilities="iso|ref|box|tag|val|trn"
    operators="and|or|xor|not"
    builtin_types="String|None|Any|Array"

    # Add the language's grammar to the static completion list
    static_words="${values} ${meta} ${keywords} ${types_decl} ${capabilities}"
    static_words="${static_words} ${operators} ${builtin_types}"
    printf %s\\n "declare-option str-list pony_static_words ${static_words}" | tr '|' ' '

    # Highlight keywords
    # The (?!') ensures that keywords with prime are highlighted as usual, for example try'
    printf %s "
        add-highlighter shared/pony/code/ regex \b(${values})\b(?!') 0:value
        add-highlighter shared/pony/code/ regex \b(${operators})\b(?!') 0:operator
        add-highlighter shared/pony/code/ regex \b(${meta})\b(?!') 0:meta
        add-highlighter shared/pony/code/ regex \b(${func_decl})(\s+(${capabilities}))?(\s+\w+)[\[(] 1:keyword 3:attribute 4:function
        add-highlighter shared/pony/code/ regex \b(${func_decl})\b(?!') 0:keyword
        add-highlighter shared/pony/code/ regex \b(${keywords})\b(?!') 0:keyword
        add-highlighter shared/pony/code/ regex \b(${capabilities})\b(?!')(!|\\^)? 1:attribute 2:attribute
    "

    # Highlight types and attributes
    printf %s "
        add-highlighter shared/pony/code/ regex '@[\w_]+\b' 0:attribute
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden pony-insert-on-new-line %<
    evaluate-commands -no-hooks -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K//\h* <ret> y jgi P }

        # wisely add `end` keyword
        evaluate-commands -save-regs x %<
            try %{ execute-keys -draft k x s ^ \h + <ret> \" x y } catch %{ reg x '' }
            try %<
                evaluate-commands -draft %<
                    # Check if previous line opens a block
                    execute-keys -draft kx <a-k>^<c-r>x(try|match|recover|repeat|object|.+\bdo$|.+\bthen$)[^0-9A-Za-z_']<ret>
                    # Check that we didn't put an end on the previous line that closes the block
                    execute-keys -draft kx <a-K> \bend$<ret>
                    # Check that we do not already have an end for this indent level which is first set via `pony-indent-on-new-line` hook
                    execute-keys -draft }i J x <a-K> ^<c-r>x(end|else|elseif|until)[^0-9A-Za-z_']<ret>
                >
                execute-keys -draft o<c-r>xend<esc> # insert a new line with containing end
            >
        >
    >
>

define-command -hidden pony-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft , K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
        # indent after line ending with: =>, do, try, then, else, =; or
        # starting with: actor, class, struct, trait, interface, if, elseif, recover, repeat
        # excluding: primitive, type -- because they are often written in a single line
        try %{ execute-keys -draft , k x <a-k> (\b(?:do|try|then|else|recover|repeat)|=>|=)$ | ^\h*(actor|class|struct|trait|interface|object)\b <ret> j <a-gt> }
        # else, end are always de-indented
        try %{ execute-keys -draft , k x <a-k> \b(else|end):$ <ret> k x s ^\h* <ret> y j x <a-k> ^<c-r>" <ret> J <a-lt> }
    }
}

§
