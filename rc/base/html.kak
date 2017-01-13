# http://w3.org/html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.html %{
    set buffer filetype html
}

hook global BufCreate .*\.xml %{
    set buffer filetype xml
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions html                  \
    comment <!--     -->                  '' \
    tag     <          >                  '' \
    style   <style\b.*?>\K  (?=</style>)  '' \
    script  <script\b.*?>\K (?=</script>) ''

add-highlighter -group /html/comment fill comment

add-highlighter -group /html/style  ref css
add-highlighter -group /html/script ref javascript

add-highlighter -group /html/tag regex </?(\w+) 1:keyword

add-highlighter -group /html/tag regions content \
    string '"' (?<!\\)(\\\\)*"      '' \
    string "'" "'"                  ''

add-highlighter -group /html/tag/content/string fill string

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden html-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden html-indent-on-char %{
    eval -draft -itersel %{
        # align closing tag to opening when alone on a line
        try %{ exec -draft <space> <a-h> s ^\h+</(\w+)>$ <ret> <a-\;> <a-?> <lt><c-r>1 <ret> s \`|.\' <ret> <a-r> 1<a-&> }
    }
}

def -hidden html-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : html-filter-around-selections <ret> }
        # indent after lines ending with opening tag
        try %{ exec -draft k <a-x> <a-k> <[^/][^>]+>$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group html-highlight global WinSetOption filetype=(?:html|xml) %{ add-highlighter ref html }

hook global WinSetOption filetype=(?:html|xml) %{
    hook window InsertEnd  .* -group html-hooks  html-filter-around-selections
    hook window InsertChar .* -group html-indent html-indent-on-char
    hook window InsertChar \n -group html-indent html-indent-on-new-line
}

hook -group html-highlight global WinSetOption filetype=(?!html|xml).* %{ remove-highlighter html }

hook global WinSetOption filetype=(?!html|xml).* %{
    remove-hooks window html-indent
    remove-hooks window html-hooks
}
