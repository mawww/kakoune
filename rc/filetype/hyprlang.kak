# https://hypr.land

hook global BufCreate .*/hypr/.*[.]conf %{
    set-option buffer filetype hyprlang
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=hyprlang %{
    require-module hyprlang

    hook window ModeChange pop:insert:.* -group hyprlang-trim-indent hyprlang-trim-indent
    hook window InsertChar .* -group hyprlang-indent hyprlang-indent-on-char
    hook window InsertChar \n -group hyprlang-indent hyprlang-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window hyprlang-.+ }
}

hook -group hyprlang-highlight global WinSetOption filetype=hyprlang %{
    add-highlighter window/hyprlang ref hyprlang
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/hyprlang }
}

provide-module hyprlang %@

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/hyprlang regions
add-highlighter shared/hyprlang/code default-region group

add-highlighter shared/hyprlang/string region '"' (?<!\\)(\\\\)*" fill string

add-highlighter shared/hyprlang/line_comment region '#' $ fill comment

add-highlighter shared/hyprlang/code/variable regex ((?<![-:])\b\w+)\s*= 1:variable
add-highlighter shared/hyprlang/code/dollarvar regex (\$\w+)\b 1:value

add-highlighter shared/hyprlang/code/builtin regex \b(true|false)\b 0:value
add-highlighter shared/hyprlang/code/binary regex \b(0b[01_]+)\b 0:value
add-highlighter shared/hyprlang/code/octal regex \b(0o[0-7_]+)\b 0:value
add-highlighter shared/hyprlang/code/hex regex \b(0x[a-fA-F0-9_]+)\b 0:value
add-highlighter shared/hyprlang/code/decimal regex \b([0-9-+][0-9_]*)\b 0:value
add-highlighter shared/hyprlang/code/float regex \b([0-9-+][0-9_]*\.[0-9_]+)\b 0:value
add-highlighter shared/hyprlang/code/float_exp regex \b([0-9-+][0-9_]*(\.[0-9_]+)?[eE][-+]?[0-9_]+)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden hyprlang-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden hyprlang-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden hyprlang-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : hyprlang-trim-indent <ret> }
        # indent after lines ending with opener token
        try %< execute-keys -draft k x <a-k> [[{]\h*$ <ret> j <a-gt> >
        # deindent closer token(s) when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
    >
>

@
