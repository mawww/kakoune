# http://graphql.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](graphql) %{
    set-option buffer filetype graphql
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=graphql %{
    require-module graphql

    hook window ModeChange pop:insert:.* -group graphql-trim-indent  graphql-trim-indent
    hook window InsertChar .* -group graphql-indent graphql-indent-on-char
    hook window InsertChar \n -group graphql-indent graphql-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window graphql-.+ }
}

hook -group graphql-highlight global WinSetOption filetype=graphql %{
    add-highlighter window/graphql ref graphql
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/graphql }
}


provide-module graphql %(

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/graphql regions
add-highlighter shared/graphql/code default-region group
add-highlighter shared/graphql/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/graphql/comment region '#' \n fill comment

add-highlighter shared/graphql/code/ regex \b(fragment|query|mutation|on)\b 0:keyword
add-highlighter shared/graphql/code/ regex \b(true|false|null|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?)\b 0:value
add-highlighter shared/graphql/code/ regex \$[a-zA-Z0-9]+\b 0:value
add-highlighter shared/graphql/code/ regex @(?:include|skip) 0:meta
add-highlighter shared/graphql/code/ regex \b(Boolean|Float|ID|Int|String)\b 0:type
add-highlighter shared/graphql/code/ regex ! 0:operator

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden graphql-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden graphql-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden graphql-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : graphql-trim-indent <ret> }
        # indent after lines ending with opener token
        try %< execute-keys -draft k <a-x> <a-k> [[{]\h*$ <ret> j <a-gt> >
        # deindent closer token(s) when after cursor
        try %< execute-keys -draft <a-x> <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

)
