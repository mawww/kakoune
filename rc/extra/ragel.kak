# http://complang.org/ragel
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# ragel.kak does not try to detect host language.

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ragel|rl) %{
    set buffer filetype ragel
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code ragel \
    string  '"' (?<!\\)(\\\\)*"         '' \
    string  "'" "'"                     '' \
    comment '#' '$'                     ''

add-highlighter -group /ragel/string  fill string
add-highlighter -group /ragel/comment fill comment

add-highlighter -group /ragel/code regex \b(true|false)\b 0:value
add-highlighter -group /ragel/code regex '%%\{|\}%%|<\w+>' 0:identifier
add-highlighter -group /ragel/code regex :=|=>|->|:>|:>>|<: 0:operator
add-highlighter -group /ragel/code regex \b(action|alnum|alpha|any|ascii|case|cntrl|contained|context|data|digit|empty|eof|err|error|exec|export|exports|extend|fblen|fbreak|fbuf|fc|fcall|fcurs|fentry|fexec|fgoto|fhold|first_final|fnext|fpc|fret|from|fstack|ftargs|graph|import|include|init|inwhen|lerr|lower|machine|nocs|noend|noerror|nofinal|noprefix|outwhen|postpop|prepush|print|punct|range|space|start|to|upper|when|write|xdigit|zlen)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _ragel_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _ragel_indent_on_char %<
    eval -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< exec -draft <a-h> <a-k> ^\h+[]})]$ <ret>        m         s \`|.\' <ret> 1<a-&> >
        try %< exec -draft <a-h> <a-k> ^\h+  [*]$ <ret> <a-?> [*]$ <ret> s \`|.\' <ret> 1<a-&> >
    >
>

def -hidden _ragel_indent_on_new_line %<
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _ragel_filter_around_selections <ret> }
        # copy _#_ comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y gh j P }
        # indent after lines ending with opener token
        try %< exec -draft k x <a-k> [[{(*]$ <ret> j <a-gt> >
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group ragel-highlight global WinSetOption filetype=ragel %{ add-highlighter ref ragel }

hook global WinSetOption filetype=ragel %{
    hook window InsertEnd  .* -group ragel-hooks  _ragel_filter_around_selections
    hook window InsertChar .* -group ragel-indent _ragel_indent_on_char
    hook window InsertChar \n -group ragel-indent _ragel_indent_on_new_line
}

hook -group ragel-highlight global WinSetOption filetype=(?!ragel).* %{ remove-highlighter ragel }

hook global WinSetOption filetype=(?!ragel).* %{
    remove-hooks window ragel-indent
    remove-hooks window ragel-hooks
}
