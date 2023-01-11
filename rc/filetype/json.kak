# http://json.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](json) %{
    set-option buffer filetype json
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=json %{
    require-module json

    hook window ModeChange pop:insert:.* -group json-trim-indent json-trim-indent
    hook window InsertChar .* -group json-indent json-indent-on-char
    hook window InsertChar \n -group json-indent json-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window json-.+ }
}

hook -group json-highlight global WinSetOption filetype=json %{
    add-highlighter window/json ref json
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/json }
}


provide-module json %(

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/json regions
add-highlighter shared/json/code default-region group
add-highlighter shared/json/string region '"' (?<!\\)(\\\\)*" fill string

add-highlighter shared/json/code/ regex \b(true|false|null|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden json-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden json-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden json-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : json-trim-indent <ret> }
        # indent after lines ending with opener token
        try %< execute-keys -draft k x <a-k> [[{]\h*$ <ret> j <a-gt> >
        # deindent closer token(s) when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

)
