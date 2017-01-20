# http://ponylang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](pony) %{
    set buffer filetype pony
}

hook global BufOpen .*[.](pony) %{
    set buffer filetype pony
}

hook global BufNew .*[.](pony) %{
    set buffer filetype pony
}

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code pony \
    double_string '"""' '"""'            '' \
    double_string '"'   (?<!\\)(\\\\)*"  '' \
    comment       '/\*'   '\*/'            '' \
    comment       '//'   '$'             ''

addhl -group /pony/double_string fill string
# addhl -group /pony/single_string fill string
addhl -group /pony/comment       fill comment


%sh{
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
    static_words="${values}:${meta}:${keywords}:${types_decl}:${capabilities}"
    static_words="${static_words}::${struct}"
    printf %s\\n "hook global WinSetOption filetype=pony %{
        set window static_words '${static_words}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /pony/code regex '\b(${values})\b' 0:value
        addhl -group /pony/code regex '\b(${meta})\b' 0:meta
        addhl -group /pony/code regex '\b(${func_decl})(\s+(${capabilities}))?(\s+\w+)\(' 1:type 3:builtin 4:builtin
        addhl -group /pony/code regex '\b(${func_decl})\b' 0:type
        addhl -group /pony/code regex '=>' 0:type
        addhl -group /pony/code regex '\b(${keywords})\b' 0:keyword
        addhl -group /pony/code regex ';' 0:keyword
        addhl -group /pony/code regex '^\s*|' 0:keyword
        addhl -group /pony/code regex '\b(${struct})\b' 0:identifier
        addhl -group /pony/code regex '\b(${capabilities})\b(!|^)?' 1:builtin 2:builtin
    "

    # Highlight types and attributes
    printf %s "
        addhl -group /pony/code regex '@[\w_]+\b' 0:attribute
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden pony-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ exec -draft k <a-x> s \h+$ <ret> d }
        # copy '//' comment prefix and following white spaces
        # try %{ exec -draft k x s ^\h*//\h* <ret> y jgh P }
        # indent after line ending with :
        try %{ exec -draft <space> k x <a-k> (do|try|then|else|:|=>)$ <ret> j <a-gt> }
        # else, end are always de-indented
        try %{ exec -draft <space> k x <a-k> (else|end):$ <ret> k x s ^\h* <ret> y j x <a-k> ^<c-r>" <ret> J <a-lt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group pony-highlight global WinSetOption filetype=pony %{ addhl ref pony }

hook global WinSetOption filetype=pony %{
    hook window InsertChar \n -group pony-indent pony-indent-on-new-line
    # cleanup trailing whitespaces on current line insert end
    hook window InsertEnd .* -group pony-indent %{ try %{ exec -draft \; <a-x> s ^\h+$ <ret> d } }
}

hook global WinSetOption filetype=pony %{
    set buffer tabstop 2
    set buffer indentwidth 2
}

hook -group pony-highlight global WinSetOption filetype=(?!pony).* %{ rmhl pony }

hook global WinSetOption filetype=(?!pony).* %{
    rmhooks window pony-indent
}
