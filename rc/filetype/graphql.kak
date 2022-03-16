# http://graphql.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](graphqls?) %{
    set-option buffer filetype graphql
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=graphql %{
    require-module graphql

    hook window ModeChange pop:insert:.* -group graphql-trim-indent graphql-trim-indent
    hook window InsertChar .* -group graphql-indent graphql-indent-on-char
    hook window InsertChar \n -group graphql-indent graphql-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window graphql-.+ }
}

hook -group graphql-highlight global WinSetOption filetype=graphql %{
    add-highlighter window/graphql ref graphql
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/graphql }
}


provide-module graphql %§ 

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/graphql regions
add-highlighter shared/graphql/code default-region group
add-highlighter shared/graphql/line-description region '#' '\n' fill comment
add-highlighter shared/graphql/block-description region '"""' '"""' fill comment
add-highlighter shared/graphql/description region '"' '"\s*\n' fill comment
add-highlighter shared/graphql/object region -recurse \{ [{] [}] regions

# Objects
add-highlighter shared/graphql/object/line-description region '#' '\n' fill comment
add-highlighter shared/graphql/object/block-description region '"""' '"""' fill comment
add-highlighter shared/graphql/object/field default-region group
add-highlighter shared/graphql/object/field/ regex ([A-Za-z][A-Za-z0-9_-]*)(?:\([^)]*\))?\h*[:{] 1:attribute
add-highlighter shared/graphql/object/field/ regex ^\h*([A-Za-z][A-Za-z0-9_-]*)\h*$ 1:attribute

# Values
add-highlighter shared/graphql/object/field/values regex \b(true|false|null|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?)\b 0:value
add-highlighter shared/graphql/object/field/variables regex \$[a-zA-Z0-9]+\b 0:variable
# add-highlighter shared/graphql/object/field/string regex '"([^"]|\\")*"' 0:string
add-highlighter shared/graphql/object/field/string regex '"(?:[^"\\]|\\.)*"' 0:string

# Meta
add-highlighter shared/graphql/object/field/directives regex @(?:include|skip) 0:meta

# Attributes
add-highlighter shared/graphql/object/field/required regex '(?<=[\w\]])(?<bang>!)' bang:operator
add-highlighter shared/graphql/object/field/assignment regex '=' 0:operator

# Keywords
add-highlighter shared/graphql/code/top-level regex '\bschema\b' 0:keyword
add-highlighter shared/graphql/code/keywords regex '\b(?<name>enum|fragment|input|implements|interface|mutation|on|query|scalar|subscription|type|union)\h+(?:[A-Za-z]\w*)' name:keyword

# Types
add-highlighter shared/graphql/object/field/scalars regex \b(Boolean|Float|ID|Int|String)\b 0:type

# Operators
add-highlighter shared/graphql/object/field/expand-fragment regex '\.\.\.(?=\w)' 0:operator

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden graphql-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
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
        try %< execute-keys -draft k x <a-k> [[{]\h*$ <ret> j <a-gt> >
        # deindent closer token(s) when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

§ 
