# http://handlebarsjs.com/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](hbs) %{
    set buffer filetype hbs
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default html hbs  \
    comment          {{!-- --}} '' \
    comment          {{!   }}   '' \
    block-expression {{    }}   '' 

addhl -group /hbs/html ref html
addhl -group /hbs/comment fill comment
addhl -group /hbs/block-expression regex {{((#|/|)(\w|-)+) 1:meta

# some hbs tags have a special meaning
addhl -group /hbs/block-expression regex {{((#|/|)(if|else|unless|with|lookup|log)) 1:keyword

# 'each' is special as it really is two words 'each' and 'as'
addhl -group /hbs/block-expression regex {{((#|/|)((each).*(as))) 2:keyword 4:keyword 5:keyword

addhl -group /hbs/block-expression regex ((\w|-)+)= 1:attribute

# highlight the string values of attributes as a bonus
addhl -group /hbs/block-expression regex ((\w|-)+)=(('|").*?('|")) 1:attribute 3:value

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _hbs_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _hbs_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _hbs_filter_around_selections <ret> }
        # copy '/' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K/\h* <ret> y j p }
        # indent after lines beginning with : or -
        try %{ exec -draft k x <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group hbs-highlight global WinSetOption filetype=hbs %{
    addhl ref hbs
}

hook global WinSetOption filetype=hbs %{
    hook window InsertEnd  .* -group hbs-hooks  _hbs_filter_around_selections
    hook window InsertChar \n -group hbs-indent _hbs_indent_on_new_line
}

hook -group hbs-highlight global WinSetOption filetype=(?!hbs).* %{
    rmhl hbs
}

hook global WinSetOption filetype=(?!hbs).* %{
    rmhooks window hbs-indent
    rmhooks window hbs-hooks
}
