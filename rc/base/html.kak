# http://w3.org/html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.html %{
    set-option buffer filetype html
}

hook global BufCreate .*\.xml %{
    set-option buffer filetype xml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions html                  \
    comment <!--     -->                  '' \
    tag     <          >                  '' \
    style   <style\b.*?>\K  (?=</style>)  '' \
    script  <script\b.*?>\K (?=</script>) ''

add-highlighter shared/html/comment fill comment

add-highlighter shared/html/style  ref css
add-highlighter shared/html/script ref javascript

add-highlighter shared/html/tag regex \b([a-zA-Z0-9_-]+)=? 1:attribute
add-highlighter shared/html/tag regex </?(\w+) 1:keyword
add-highlighter shared/html/tag regex <(!DOCTYPE(\h+\w+)+) 1:meta

add-highlighter shared/html/tag regions content \
    string '"' (?<!\\)(\\\\)*"      '' \
    string "'" "'"                  ''

add-highlighter shared/html/tag/content/string fill string

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden html-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden html-indent-on-greater-than %[
    evaluate-commands -draft -itersel %[
        # align closing tag to opening when alone on a line
        try %[ execute-keys -draft <space> <a-h> s ^\h+<lt>/(\w+)<gt>$ <ret> {c<lt><c-r>1,<lt>/<c-r>1<gt> <ret> s \A|.\z <ret> 1<a-&> ]
    ]
]

define-command -hidden html-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : html-filter-around-selections <ret> }
        # indent after lines ending with opening tag
        try %{ execute-keys -draft k <a-x> <a-k> <[^/][^>]+>$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group html-highlight global WinSetOption filetype=(?:html|xml) %{ add-highlighter window ref html }

hook global WinSetOption filetype=(?:html|xml) %{
    hook window InsertEnd  .* -group html-hooks  html-filter-around-selections
    hook window InsertChar '>' -group html-indent html-indent-on-greater-than
    hook window InsertChar \n -group html-indent html-indent-on-new-line
}

hook -group html-highlight global WinSetOption filetype=(?!html)(?!xml).* %{ remove-highlighter window/html }

hook global WinSetOption filetype=(?!html)(?!xml).* %{
    remove-hooks window html-indent
    remove-hooks window html-hooks
}
