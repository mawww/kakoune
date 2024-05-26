hook global BufCreate .*[.](tcl) %{
    set-option buffer filetype tcl
}

hook global WinSetOption filetype=tcl %{
    require-module tcl

    hook window ModeChange pop:insert:.* -group tcl-trim-indent tcl-trim-indent
    hook window InsertChar \n -group tcl-insert tcl-insert-on-new-line
    hook window InsertChar \n -group tcl-indent tcl-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window tcl-.+ }
}

hook -group tcl-highlight global WinSetOption filetype=tcl %{
    add-highlighter window/tcl ref tcl
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/tcl }
}

# Using non-ascii characters here so that we can use the '[' command
provide-module tcl %§

add-highlighter shared/tcl regions
add-highlighter shared/tcl/code default-region group
add-highlighter shared/tcl/comment region (?<!\\)(?:\\\\)*(?:^|\h)\K# '$' fill comment
add-highlighter shared/tcl/double_string region  %{(?<!\\)(?:\\\\)*\K"} %{(?<!\\)(?:\\\\)*"} group
add-highlighter shared/tcl/double_string/fill fill string

evaluate-commands %sh{
    # Tcl does not have keywords, as everything in Tcl is a command.
    # Highlight all builtin commads does not make a lot of sense as there are plenty of them.
    # What is more, sometimes user defines varaibles have the same names as commands.
    # On the other hand, highlight no commads makes the code harder to read.
    # The approach for builtin commads highlighting is very simply.
    # Highlight only two types of commands:
    #   1. "Control statement" like commands, for exmaple, "if", "break", "return", etc.
    #   2. Commands defining new scope, for exmaple, "proc", "namespace".
    keywords="break catch continue default else elseif error exit for foreach if return switch while proc namespace"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Highlight keywords
    printf %s\\n "add-highlighter shared/tcl/code/ regex (?<!-)\b($(join "${keywords}" '|'))\b(?!-) 0:keyword"
}

add-highlighter shared/tcl/code/function   regex ^\h*proc\h+((\w|-)+) 1:function
add-highlighter shared/tcl/code/brackets   regex [\[\]]{1,2}          0:operator
add-highlighter shared/tcl/code/parameters regex \s-\w+\b             0:attribute
add-highlighter shared/tcl/code/variable   regex \$(\w|:)+            0:variable
add-highlighter shared/tcl/code/numbers    regex '\b\d+\.?'           0:value

add-highlighter shared/tcl/double_string/variable regex \$(\w|:)+   0:variable
add-highlighter shared/tcl/double_string/brackets regex [\[\]]{1,2} 0:operator

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden tcl-trim-indent %{
    # Remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden tcl-insert-on-new-line %[
    # Copy '#' comment prefix and following white spaces
    try %{ execute-keys -draft k x s ^\h*\K#\h* <ret> y gh j P }
]

define-command -hidden tcl-indent-on-new-line %¶
    evaluate-commands -draft -itersel %@
        # Preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }

        # Filter previous line
        try %{ execute-keys -draft k : tcl-trim-indent <ret> }

        # Indent after {
        try %= execute-keys -draft , k x <a-k> (\s|^)\{$ <ret> j <a-gt> =
    @
¶
§
