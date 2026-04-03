# https://snakemake.github.io
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# SnakeMake is a pipeline/workflow management system.

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.([Ss]nake[Ff]ile) %{
    set-option buffer filetype snakefile
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=snakefile %{
    require-module snakefile

    hook window ModeChange pop:insert:.* -group snakefile-trim-indent snakefile-trim-indent
    hook window InsertChar \n -group snakefile-insert snakefile-insert-on-new-line
    hook window InsertChar \n -group snakefile-indent snakefile-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window snakefile-.+ }
}

hook -group snakefile-highlight global WinSetOption filetype=snakefile %{
    add-highlighter window/snakefile ref snakefile
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/snakefile }
}


provide-module snakefile %{

require-module python

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/snakefile regions
add-highlighter shared/snakefile/comment  region '(?:^| )#' $   fill comment

add-highlighter shared/snakefile/code  default-region group
add-highlighter shared/snakefile/code/ regex \bconfig\b    0:builtin
add-highlighter shared/snakefile/code/ regex \bwildcards\b 0:builtin
add-highlighter shared/snakefile/code/ regex ^(\w+): 1:attribute
add-highlighter shared/snakefile/code/ regex ^(rule)\s+(\w+): 1:keyword 2:module
add-highlighter shared/snakefile/code/ regex ^\s{4}(\w+):   1:attribute

add-highlighter shared/snakefile/section  region ^\s{8} \n  group
add-highlighter shared/snakefile/section/ ref python
add-highlighter shared/snakefile/section/f_string regex \{.*?\} 0:value
add-highlighter shared/snakefile/section/keywords regex \bconfig\b    0:builtin  # FIXME
add-highlighter shared/snakefile/section/keywords regex \bwildcards\b 0:builtin  # FIXME

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden snakefile-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden snakefile-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
    }
}

define-command -hidden snakefile-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : snakefile-trim-indent <ret> }
        # indent after :
        try %{ execute-keys -draft , k x <a-k> :$ <ret> j <a-gt> }
    }
}

}

