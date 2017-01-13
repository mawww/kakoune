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

def -hidden hbs-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden hbs-indent-on-new-line %{
    eval -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K/\h* <ret> y j p }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : hbs-filter-around-selections <ret> }
        # indent after lines beginning with : or -
        try %{ exec -draft k <a-x> <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hbs-highlight global WinSetOption filetype=hbs %{
    add-highlighter ref hbs
}

hook global WinSetOption filetype=hbs %{
    hook window InsertEnd  .* -group hbs-hooks  hbs-filter-around-selections
    hook window InsertChar \n -group hbs-indent hbs-indent-on-new-line
}

hook -group hbs-highlight global WinSetOption filetype=(?!hbs).* %{
    remove-highlighter hbs
}

hook global WinSetOption filetype=(?!hbs).* %{
    remove-hooks window hbs-indent
    remove-hooks window hbs-hooks
}
