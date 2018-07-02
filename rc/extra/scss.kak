# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require css.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scss) %{
    set-option buffer filetype scss
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/scss regions
add-highlighter shared/scss/core default-region group
add-highlighter shared/scss/comment region // $ fill comment

add-highlighter shared/scss/core/ ref css
add-highlighter shared/scss/core/ regex @[A-Za-z][A-Za-z0-9_-]* 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden scss-filter-around-selections      css-filter-around-selections
define-command -hidden scss-indent-on-new-line            css-indent-on-new-line
define-command -hidden scss-indent-on-closing-curly-brace css-indent-on-closing-curly-brace

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scss-highlight global WinSetOption filetype=scss %{ add-highlighter window/scss ref scss }

hook global WinSetOption filetype=scss %[
    hook window ModeChange insert:.* -group scss-hooks  scss-filter-around-selections
    hook window InsertChar \n -group scss-indent scss-indent-on-new-line
    hook window InsertChar \} -group scss-indent scss-indent-on-closing-curly-brace
    set-option buffer extra_word_chars '-'
]

hook -group scss-highlight global WinSetOption filetype=(?!scss).* %{ remove-highlighter window/scss }

hook global WinSetOption filetype=(?!scss).* %{
    remove-hooks window scss-indent
    remove-hooks window scss-hooks
}
