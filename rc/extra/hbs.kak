# http://handlebarsjs.com/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](hbs) %{
    set buffer filetype hbs
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default html hbs  \
    comment          {{!-- --}} '' \
    comment          {{!   }}   '' \
    block-expression {{    }}   '' 

add-highlighter -group /hbs/html ref html
add-highlighter -group /hbs/comment fill comment
add-highlighter -group /hbs/block-expression regex {{((#|/|)(\w|-)+) 1:meta

# some hbs tags have a special meaning
add-highlighter -group /hbs/block-expression regex {{((#|/|)(if|else|unless|with|lookup|log)) 1:keyword

# 'each' is special as it really is two words 'each' and 'as'
add-highlighter -group /hbs/block-expression regex {{((#|/|)((each).*(as))) 2:keyword 4:keyword 5:keyword

add-highlighter -group /hbs/block-expression regex ((\w|-)+)= 1:attribute

# highlight the string values of attributes as a bonus
add-highlighter -group /hbs/block-expression regex ((\w|-)+)=(('|").*?('|")) 1:attribute 3:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _hbs_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _hbs_indent_on_new_line %{
    eval -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K/\h* <ret> y j p }
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _hbs_filter_around_selections <ret> }
        # indent after lines beginning with : or -
        try %{ exec -draft k x <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hbs-highlight global WinSetOption filetype=hbs %{
    add-highlighter ref hbs
}

hook global WinSetOption filetype=hbs %{
    hook window InsertEnd  .* -group hbs-hooks  _hbs_filter_around_selections
    hook window InsertChar \n -group hbs-indent _hbs_indent_on_new_line
}

hook -group hbs-highlight global WinSetOption filetype=(?!hbs).* %{
    remove-highlighter hbs
}

hook global WinSetOption filetype=(?!hbs).* %{
    remove-hooks window hbs-indent
    remove-hooks window hbs-hooks
}
