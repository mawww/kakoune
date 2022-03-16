# https://github.com/toml-lang/toml/tree/v0.4.0
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(toml) %{
    set-option buffer filetype toml
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=toml %{
    require-module toml

    hook window ModeChange pop:insert:.* -group toml-trim-indent toml-trim-indent
    hook window InsertChar \n -group toml-insert toml-insert-on-new-line
    hook window InsertChar \n -group toml-indent toml-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window toml-.+ }
}

hook -group toml-highlight global WinSetOption filetype=toml %{
    add-highlighter window/toml ref toml
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/toml }
}


provide-module toml %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/toml regions
add-highlighter shared/toml/code default-region group
add-highlighter shared/toml/comment region '#'   $           fill comment
add-highlighter shared/toml/string1 region  '"""' (?<!\\)(\\\\)*"""(?!") fill string
add-highlighter shared/toml/string2 region  "'''" "'''(?!')"             fill string
add-highlighter shared/toml/string3 region  '"'   (?<!\\)(\\\\)*"        fill string
add-highlighter shared/toml/string4 region  "'"   "'"                    fill string

add-highlighter shared/toml/code/ regex \
    "^\h*\[\[?([A-Za-z0-9._-]*)\]\]?" 1:title
add-highlighter shared/toml/code/ regex \
    (?<!\w)[+-]?[0-9](_?\d)*(\.[0-9](_?\d)*)?([eE][+-]?[0-9](_?\d)*)?\b 0:value
add-highlighter shared/toml/code/ regex \
    true|false 0:value
add-highlighter shared/toml/code/ regex \
    '\d{4}-\d{2}-\d{2}[Tt ]\d{2}:\d{2}:\d{2}(.\d+)?([Zz]|[+-]\d{2}:\d{2})' 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden toml-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden toml-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy # comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
    }
}

define-command -hidden toml-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : toml-trim-indent <ret> }
    }
}

}
