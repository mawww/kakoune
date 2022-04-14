# http://yaml.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](ya?ml) %{
    set-option buffer filetype yaml
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=yaml %{
    require-module yaml

    hook window ModeChange pop:insert:.* -group yaml-trim-indent yaml-trim-indent
    hook window InsertChar \n -group yaml-insert yaml-insert-on-new-line
    hook window InsertChar \n -group yaml-indent yaml-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window yaml-.+ }
}

hook -group yaml-highlight global WinSetOption filetype=yaml %{
    add-highlighter window/yaml ref yaml
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/yaml }
}


provide-module yaml %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/yaml regions
add-highlighter shared/yaml/code      default-region group
add-highlighter shared/yaml/double_string region '"' (?<!\\)(\\\\)*"       fill string
add-highlighter shared/yaml/single_string region "'" "'"                   fill string
add-highlighter shared/yaml/comment       region '(?:^| )#' $              fill comment

add-highlighter shared/yaml/code/ regex ^(---|\.\.\.)$ 0:meta
add-highlighter shared/yaml/code/ regex ^(\h*:\w*) 0:keyword
add-highlighter shared/yaml/code/ regex \b(true|false|null)\b 0:value
add-highlighter shared/yaml/code/ regex ^\h*-?\h*(\S+): 1:attribute

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden yaml-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden yaml-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
    }
}

define-command -hidden yaml-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : yaml-trim-indent <ret> }
        # indent after :
        try %{ execute-keys -draft , k x <a-k> :$ <ret> j <a-gt> }
        # indent after -
        try %{ execute-keys -draft , k x <a-k> ^\h*- <ret> j <a-gt> }
    }
}

}
