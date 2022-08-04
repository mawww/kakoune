# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scss) %{
    set-option buffer filetype scss
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=scss %[
    require-module scss

    hook window ModeChange pop:insert:.* -group scss-trim-indent scss-trim-indent
    hook window InsertChar \n -group scss-indent scss-insert-on-new-line
    hook window InsertChar \n -group scss-indent scss-indent-on-new-line
    hook window InsertChar \} -group scss-indent scss-indent-on-closing-curly-brace
    set-option buffer extra_word_chars '_' '-'

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window scss-.+ }
]

hook -group scss-highlight global WinSetOption filetype=scss %{
    add-highlighter window/scss ref scss
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/scss }
}


provide-module scss %[

require-module css

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/scss regions
add-highlighter shared/scss/core default-region group
add-highlighter shared/scss/comment region ^\h*// $ fill comment

add-highlighter shared/scss/core/ ref css
add-highlighter shared/scss/core/ regex & 0:keyword
add-highlighter shared/scss/core/ regex \$[A-Za-z][A-Za-z0-9_-]* 0:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden scss-trim-indent      css-trim-indent
define-command -hidden scss-insert-on-new-line            css-insert-on-new-line
define-command -hidden scss-indent-on-new-line            css-indent-on-new-line
define-command -hidden scss-indent-on-closing-curly-brace css-indent-on-closing-curly-brace

]
