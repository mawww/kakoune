# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](sass) %{
    set-option buffer filetype sass
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/sass regions
add-highlighter shared/sass/code default-region group
add-highlighter shared/sass/single_string  region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/sass/double_string  region "'" "'"             fill string
add-highlighter shared/sass/comment        region '/' '$'             fill comment

add-highlighter shared/sass/code/ regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:variable
add-highlighter shared/sass/code/ regex &|@[A-Za-z][A-Za-z0-9_-]* 0:meta
add-highlighter shared/sass/code/ regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(em|px)) 0:value
add-highlighter shared/sass/code/ regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
add-highlighter shared/sass/code/ regex :(before|after) 0:attribute
add-highlighter shared/sass/code/ regex !important 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden sass-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden sass-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K/\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : sass-filter-around-selections <ret> }
        # avoid indent after properties and comments
        try %{ execute-keys -draft k <a-x> <a-K> [:/] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group sass-highlight global WinSetOption filetype=sass %{ add-highlighter window/sass ref sass }

hook global WinSetOption filetype=sass %{
    hook window ModeChange insert:.* -group sass-hooks  sass-filter-around-selections
    hook window InsertChar \n -group sass-indent sass-indent-on-new-line
    set-option buffer extra_word_chars '-'
}

hook -group sass-highlight global WinSetOption filetype=(?!sass).* %{ remove-highlighter window/sass }

hook global WinSetOption filetype=(?!sass).* %{
    remove-hooks window sass-indent
    remove-hooks window sass-hooks
}
