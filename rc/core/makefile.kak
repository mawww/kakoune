# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*/?[mM]akefile %{
    set buffer filetype makefile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default content makefile \
   comment '#' '$' '' \
   eval '\$\(' '\)' '\('

add-highlighter -group /makefile/comment fill comment
add-highlighter -group /makefile/eval fill value

add-highlighter -group /makefile/content regex ^[\w.%-]+\h*:\s 0:identifier
add-highlighter -group /makefile/content regex [+?:]= 0:operator

%sh{
    # Grammar
    keywords="ifeq|ifneq|ifdef|ifndef|else|endif|define|endef"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=makefile %{
        set window static_words '${keywords}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "add-highlighter -group /makefile/content regex \b(${keywords})\b 0:keyword"
}

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden makefile-indent-on-new-line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft \;K<a-&> }
        ## If the line above is a target indent with a tab
        try %{ exec -draft Z k<a-x> <a-k>^[^:]+:\s<ret> z i<tab> }
        # cleanup trailing white space son previous line
        try %{ exec -draft k<a-x> s \h+$ <ret>d }
        # indent after some keywords
        try %{ exec -draft Z k<a-x> <a-k> ^\h*(ifeq|ifneq|ifdef|ifndef|else|define)\b<ret> z <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group makefile-highlight global WinSetOption filetype=makefile %{ add-highlighter ref makefile }

hook global WinSetOption filetype=makefile %{
    hook window InsertChar \n -group makefile-indent makefile-indent-on-new-line
}

hook -group makefile-highlight global WinSetOption filetype=(?!makefile).* %{ remove-highlighter makefile }

hook global WinSetOption filetype=(?!makefile).* %{
    remove-hooks window makefile-indent
}
