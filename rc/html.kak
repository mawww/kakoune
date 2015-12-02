# http://w3.org/html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-html %{
    set buffer filetype html
}

hook global BufCreate .*[.](html) %{
    set buffer filetype html
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions html                  \
    comment <!--     -->                  '' \
    tag     <          >                  '' \
    style   <style\>.*?>\K  (?=</style>)  '' \
    script  <script\>.*?>\K (?=</script>) ''

addhl -group /html/comment fill comment

addhl -group /html/style  ref css
addhl -group /html/script ref javascript

addhl -group /html/tag regex </?(\w+) 1:keyword

addhl -group /html/tag regions content \
    string '"' (?<!\\)(\\\\)*"      '' \
    string "'" "'"                  ''

addhl -group /html/tag/content/string fill string

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _html_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _html_indent_on_char %{
    eval -draft -itersel %{
        # align closing tag to opening when alone on a line
        try %{ exec -draft <space> <a-h> s ^\h+</(\w+)>$ <ret> <a-\;> <a-?> <lt><c-r>1 <ret> s \`|.\' <ret> <a-r> 1<a-&> }
    }
}

def -hidden _html_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _html_filter_around_selections <ret> }
        # indent after lines ending with opening tag
        try %{ exec -draft k x <a-k> <[^/][^>]+>$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=html %{
    addhl ref html

    hook window InsertEnd  .* -group html-hooks  _html_filter_around_selections
    hook window InsertChar .* -group html-indent _html_indent_on_char
    hook window InsertChar \n -group html-indent _html_indent_on_new_line

    set window comment_selection_chars '<!--:-->'
}

hook global WinSetOption filetype=(?!html).* %{
    rmhl html
    rmhooks window html-indent
    rmhooks window html-hooks
}
