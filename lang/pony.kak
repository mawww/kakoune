# http://ponylang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](pony) %{
    set-option buffer filetype pony
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/pony regions
add-highlighter shared/pony/code default-region group
add-highlighter shared/pony/triple_string region '"""' '"""'            fill string
add-highlighter shared/pony/double_string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/pony/comment       region '/\*'   '\*/'          fill comment
add-highlighter shared/pony/line_comment  region '//'   '$'             fill comment

evaluate-commands %sh{
    # Grammar
    values="true|false|None|this"
    meta='use'
    # Keyword list is collected using `keyword.kwlist` from `keyword`
    keywords="and|as|or|break|match|continue|else|try|in|return|end|for|is"
    keywords="${keywords}|recover|consume|error|do|then|if|while"
    func_decl="new|fun|be|lambda"
    capabilities="iso|ref|box|tag|val|trn"
    struct="class|actor|interface|trait|primitive|type|var|let|embed"


    # Add the language's grammar to the static completion list
    static_words="${values} ${meta} ${keywords} ${types_decl} ${capabilities}"
    static_words="${static_words} ${struct}"
    printf %s\\n "hook global WinSetOption filetype=pony %{
        set-option window static_words ${static_words}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "
        add-highlighter shared/pony/code/ regex '\b(${values})\b' 0:value
        add-highlighter shared/pony/code/ regex '\b(${meta})\b' 0:meta
        add-highlighter shared/pony/code/ regex '\b(${func_decl})(\s+(${capabilities}))?(\s+\w+)\(' 1:type 3:builtin 4:builtin
        add-highlighter shared/pony/code/ regex '\b(${func_decl})\b' 0:type
        add-highlighter shared/pony/code/ regex '=>' 0:type
        add-highlighter shared/pony/code/ regex '\b(${keywords})\b' 0:keyword
        add-highlighter shared/pony/code/ regex ';' 0:keyword
        add-highlighter shared/pony/code/ regex '^\s*|' 0:keyword
        add-highlighter shared/pony/code/ regex '\b(${struct})\b' 0:variable
        add-highlighter shared/pony/code/ regex '\b(${capabilities})\b(!|^)?' 1:builtin 2:builtin
    "

    # Highlight types and attributes
    printf %s "
        add-highlighter shared/pony/code/ regex '@[\w_]+\b' 0:attribute
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden pony-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <space> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k <a-x> s \h+$ <ret> d }
        # copy '//' comment prefix and following white spaces
        # try %{ execute-keys -draft k x s ^\h*//\h* <ret> y jgh P }
        # indent after line ending with :
        try %{ execute-keys -draft <space> k x <a-k> (do|try|then|else|:|=>)$ <ret> j <a-gt> }
        # else, end are always de-indented
        try %{ execute-keys -draft <space> k x <a-k> (else|end):$ <ret> k x s ^\h* <ret> y j x <a-k> ^<c-r>" <ret> J <a-lt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group pony-highlight global WinSetOption filetype=pony %{ add-highlighter window/pony ref pony }

hook global WinSetOption filetype=pony %{
    hook window InsertChar \n -group pony-indent pony-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window ModeChange insert:.* -group pony-indent %{ try %{ execute-keys -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook -group pony-highlight global WinSetOption filetype=(?!pony).* %{ remove-highlighter pony }

hook global WinSetOption filetype=(?!pony).* %{
    remove-hooks window pony-indent
}
