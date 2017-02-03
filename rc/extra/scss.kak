# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require css.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scss) %{
    set buffer filetype scss
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default core scss \
    comment // $ ''

add-highlighter -group /scss/comment fill comment

add-highlighter -group /scss/core ref css

add-highlighter -group /scss/core regex @[A-Za-z][A-Za-z0-9_-]* 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden scss-filter-around-selections      css-filter-around-selections
def -hidden scss-indent-on-new-line            css-indent-on-new-line
def -hidden scss-indent-on-closing-curly-brace css-indent-on-closing-curly-brace

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scss-highlight global WinSetOption filetype=scss %{ add-highlighter ref scss }

hook global WinSetOption filetype=scss %[
    hook window InsertEnd  .* -group scss-hooks  scss-filter-around-selections
    hook window InsertChar \n -group scss-indent scss-indent-on-new-line
    hook window InsertChar \} -group scss-indent scss-indent-on-closing-curly-brace
]

hook -group scss-highlight global WinSetOption filetype=(?!scss).* %{ remove-highlighter scss }

hook global WinSetOption filetype=(?!scss).* %{
    remove-hooks window scss-indent
    remove-hooks window scss-hooks
}
