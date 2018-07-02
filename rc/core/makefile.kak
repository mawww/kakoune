# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?[mM]akefile %{
    set-option buffer filetype makefile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/makefile regions

add-highlighter shared/makefile/content default-region group
add-highlighter shared/makefile/comment region '#' '$' fill comment
add-highlighter shared/makefile/evaluate-commands region -recurse '\(' '\$\(' '\)' fill value

add-highlighter shared/makefile/content/ regex ^[\w.%-]+\h*:\s 0:variable
add-highlighter shared/makefile/content/ regex [+?:]= 0:operator

evaluate-commands %sh{
    # Grammar
    keywords="ifeq|ifneq|ifdef|ifndef|else|endif|define|endef"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=makefile %{
        set-option window static_words ${keywords}
    }" | tr '|' ' '

    # Highlight keywords
    printf %s "add-highlighter shared/makefile/content/ regex \b(${keywords})\b 0:keyword"
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden makefile-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \;K<a-&> }
        ## If the line above is a target indent with a tab
        try %{ execute-keys -draft Z k<a-x> <a-k>^[^:]+:\s<ret> z i<tab> }
        # cleanup trailing white space son previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
        # indent after some keywords
        try %{ execute-keys -draft Z k<a-x> <a-k> ^\h*(ifeq|ifneq|ifdef|ifndef|else|define)\b<ret> z <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group makefile-highlight global WinSetOption filetype=makefile %{ add-highlighter window/makefile ref makefile }

hook global WinSetOption filetype=makefile %{
    hook window InsertChar \n -group makefile-indent makefile-indent-on-new-line
}

hook -group makefile-highlight global WinSetOption filetype=(?!makefile).* %{ remove-highlighter window/makefile }

hook global WinSetOption filetype=(?!makefile).* %{
    remove-hooks window makefile-indent
}
