# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*(/?[mM]akefile|\.mk|\.make) %{
    set-option buffer filetype makefile
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=makefile %{
    require-module makefile

    set-option window static_words %opt{makefile_static_words}

    hook window ModeChange pop:insert:.* -group makefile-trim-indent makefile-trim-indent
    hook window InsertChar \n -group makefile-indent makefile-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window makefile-.+ }
}

hook -group makefile-highlight global WinSetOption filetype=makefile %{
    add-highlighter window/makefile ref makefile
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/makefile }
}

provide-module makefile %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/makefile regions

add-highlighter shared/makefile/content default-region group
add-highlighter shared/makefile/comment region (?<!\\)(?:\\\\)*(?:^|\h)\K# '$' fill comment
add-highlighter shared/makefile/evaluate-commands region -recurse \( (?<!\$)(?:\$\$)*\K\$\( \) fill value

add-highlighter shared/makefile/content/ regex ^\S.*?(::|:|!)\s 0:variable
add-highlighter shared/makefile/content/ regex [+?:]= 0:operator

evaluate-commands %sh{
    # Grammar
    keywords="ifeq|ifneq|ifdef|ifndef|else|endif|define|endef"

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list makefile_static_words ${keywords}" | tr '|' ' '

    # Highlight keywords
    printf %s "add-highlighter shared/makefile/content/ regex \b(${keywords})\b 0:keyword"
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden makefile-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        execute-keys x
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden makefile-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        ## If the line above is a target indent with a tab
        try %{ execute-keys -draft Z kx <a-k>^\S.*?(::|:|!)\s<ret> z i<tab> }
        # cleanup trailing white space son previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        # indent after some keywords
        try %{ execute-keys -draft Z kx <a-k> ^\h*(ifeq|ifneq|ifdef|ifndef|else|define)\b<ret> z <a-gt> }
    }
}

}
