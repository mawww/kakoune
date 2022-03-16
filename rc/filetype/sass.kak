# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](sass) %{
    set-option buffer filetype sass
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=sass %<
    require-module sass

    hook window ModeChange pop:insert:.* -group sass-trim-indent sass-trim-indent
    hook window InsertChar \} -group sass-indent sass-indent-on-closing-brace
    hook window InsertChar \n -group sass-insert sass-insert-on-new-line
    hook window InsertChar \n -group sass-indent sass-indent-on-new-line
    set-option buffer extra_word_chars '_' '-'

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window sass-.+ }
>

hook -group sass-highlight global WinSetOption filetype=sass %{
    add-highlighter window/sass ref sass
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/sass }
}


provide-module sass %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/sass regions
add-highlighter shared/sass/code default-region group
add-highlighter shared/sass/single_string  region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/sass/double_string  region "'" "'"             fill string
add-highlighter shared/sass/comment        region '//' '$'            fill comment
add-highlighter shared/sass/css_comment    region /[*] [*]/           fill comment

add-highlighter shared/sass/code/ regex [*]|[#.][A-Za-z][A-Za-z0-9_-]* 0:variable
add-highlighter shared/sass/code/ regex &|@[A-Za-z][A-Za-z0-9_-]* 0:meta
add-highlighter shared/sass/code/ regex (#[0-9A-Fa-f]+)|((\d*\.)?\d+(em|px)) 0:value
add-highlighter shared/sass/code/ regex ([A-Za-z][A-Za-z0-9_-]*)\h*: 1:keyword
add-highlighter shared/sass/code/ regex :(before|after) 0:attribute
add-highlighter shared/sass/code/ regex !important 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden sass-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden sass-indent-on-closing-brace %<
    evaluate-commands -draft -itersel %<
        # align closing brace to same indentation as the line that the opening brace resides on
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> m <a-S> 1<a-&> ]
    >
>

define-command -hidden sass-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy // comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K/{2,}\h* <ret> y gh j P }
    >
>

define-command -hidden sass-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : sass-trim-indent <ret> }
        # avoid indent after properties and comments
        try %{ execute-keys -draft k x <a-K> [:/] <ret> j <a-gt> }
        # deindent closing brace when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> ]
    >
>

§
