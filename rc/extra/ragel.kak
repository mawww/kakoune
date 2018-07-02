# http://complang.org/ragel
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# ragel.kak does not try to detect host language.

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ragel|rl) %{
    set-option buffer filetype ragel
}

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

define-command -hidden ragel-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden ragel-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[]})]$ <ret>        m         s \A|.\z <ret> 1<a-&> >
        try %< execute-keys -draft <a-h> <a-k> ^\h+  [*]$ <ret> <a-?> [*]$ <ret> s \A|.\z <ret> 1<a-&> >
    >
>

define-command -hidden ragel-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy _#_ comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : ragel-filter-around-selections <ret> }
        # indent after lines ending with opener token
        try %< execute-keys -draft k <a-x> <a-k> [[{(*]$ <ret> j <a-gt> >
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group ragel-highlight global WinSetOption filetype=ragel %{ add-highlighter window/ragel ref ragel }

hook global WinSetOption filetype=ragel %{
    hook window ModeChange insert:.* -group ragel-hooks  ragel-filter-around-selections
    hook window InsertChar .* -group ragel-indent ragel-indent-on-char
    hook window InsertChar \n -group ragel-indent ragel-indent-on-new-line
}

hook -group ragel-highlight global WinSetOption filetype=(?!ragel).* %{ remove-highlighter window/ragel }

hook global WinSetOption filetype=(?!ragel).* %{
    remove-hooks window ragel-indent
    remove-hooks window ragel-hooks
}
