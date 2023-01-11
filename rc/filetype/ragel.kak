# http://complang.org/ragel
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# ragel.kak does not try to detect host language.

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ragel|rl) %{
    set-option buffer filetype ragel
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=ragel %{
    require-module ragel

    hook window ModeChange pop:insert:.* -group ragel-trim-indent ragel-trim-indent
    hook window InsertChar .* -group ragel-indent ragel-indent-on-char
    hook window InsertChar \n -group ragel-insert ragel-insert-on-new-line
    hook window InsertChar \n -group ragel-indent ragel-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window ragel-.+ }
}

hook -group ragel-highlight global WinSetOption filetype=ragel %{
    add-highlighter window/ragel ref ragel
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/ragel }
}

provide-module ragel %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ragel regions
add-highlighter shared/ragel/code default-region group
add-highlighter shared/ragel/double_string region '"' (?<!\\)(\\\\)*"         fill string
add-highlighter shared/ragel/single_string region "'" "'"                     fill string
add-highlighter shared/ragel/comment region '#' '$'                     fill comment

add-highlighter shared/ragel/code/ regex \b(true|false)\b 0:value
add-highlighter shared/ragel/code/ regex '%%\{|\}%%|<\w+>' 0:variable
add-highlighter shared/ragel/code/ regex :=|=>|->|:>|:>>|<: 0:operator
add-highlighter shared/ragel/code/ regex \b(action|alnum|alpha|any|ascii|case|cntrl|contained|context|data|digit|empty|eof|err|error|exec|export|exports|extend|fblen|fbreak|fbuf|fc|fcall|fcurs|fentry|fexec|fgoto|fhold|first_final|fnext|fpc|fret|from|fstack|ftargs|graph|import|include|init|inwhen|lerr|lower|machine|nocs|noend|noerror|nofinal|noprefix|outwhen|postpop|prepush|print|punct|range|space|start|to|upper|when|write|xdigit|zlen)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden ragel-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden ragel-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]})]$ <ret>        m         s \A|.\z <ret> 1<a-&> >
        try %< execute-keys -draft <a-h> <a-k> ^\h+   [*]$ <ret> <a-?> [*]$ <ret> s \A|.\z <ret> 1<a-&> >
    >
>

define-command -hidden ragel-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy _#_ comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
    >
>

define-command -hidden ragel-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ragel-trim-indent <ret> }
        # indent after lines ending with opener token
        try %< execute-keys -draft k x <a-k> [[{(*]$ <ret> j <a-gt> >
        # align closer token to its opener when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[})\]] <ret> gh / [})\]] <ret> m <a-S> 1<a-&> >
    >
>

§
