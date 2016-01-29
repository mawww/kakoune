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

def -hidden -params 1 _html_select_tag %{
    ## Select everything till the closest opened bracket followed by the name of the tag
    ## Select everything till the closed bracket followed by a slash sign and the name of the tag
    ## Extend the selection to the next closed bracket (closing tag)
    exec "
        <a-?><lt>%arg{1}\><ret>
        ; ?<lt>/%arg{1}\><ret>
        ?><ret>
    "
}

def -params 1 html-select-tag-outer -docstring "Select the closest HTML tag from the cursor, and it's content (first parameter: name of the tag)" %{
    try %{
        eval _html_select_tag %arg{1}
    } catch %{
        echo -color Error No such tag
    }
}

def -params 1 html-select-tag-inner -docstring "Select the content of the closest HTML tag from the cursor (first parameter: name of the tag)" %{
    try %{
        eval _html_select_tag %arg{1}
        ## Since the entire tag is selected after the call to the above function, extending will remove from the current selection
        ## Extend the selection to the closest opened bracket (closing tag)
        ## Extend the selection to the closest closed bracket (opening tag)
        exec %{
            <a-F><lt>H
            <a-;>
            F>L
        }

        ## The following two scopes will move the selection to the next/previous line if the closing/opening tag are at the end/beginning of a line
        ## I.e. "<tag>$" or "^/<tag>"
        try %{
            exec -draft s\`><ret>
            exec J<a-H>
        }
        try %{
            exec -draft s<lt>\'<ret>
            exec <a-\;>K<a-L>
        }
    } catch %{
        echo -color Error No such tag
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
