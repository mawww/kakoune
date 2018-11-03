# http://handlebarsjs.com/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](hbs) %{
    set-option buffer filetype hbs
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/hbs regions
add-highlighter shared/hbs/comment          region \{\{!-- --\}\} fill comment
add-highlighter shared/hbs/comment_alt      region \{\{!   \}\}   fill comment
add-highlighter shared/hbs/block-expression region \{\{[#/]   \}\}   regions
add-highlighter shared/hbs/expression 		region \{\{    \}\}   regions

define-command -hidden add-mutual-highlighters -params 1 %~
    add-highlighter "shared/hbs/%arg{1}/code" default-region group
    add-highlighter "shared/hbs/%arg{1}/single-quote" region '"'    (?<!\\)(\\\\)*" fill string
    add-highlighter "shared/hbs/%arg{1}/double-quote" region "'"    (?<!\\)(\\\\)*' fill string
    add-highlighter "shared/hbs/%arg{1}/code/variable" regex \b([\w-]+)\b 1:variable
    add-highlighter "shared/hbs/%arg{1}/code/attribute" regex \b([\w-]+)= 1:attribute
    add-highlighter "shared/hbs/%arg{1}/code/helper" regex (?:(?:\{\{)|\()([#/]?[\w-]+(?:/[\w-]+)*) 1:keyword
~

add-mutual-highlighters expression
add-mutual-highlighters block-expression

add-highlighter shared/hbs/block-expression/code/yield regex \b(as)\s|[\w-]+|\}\} 1:keyword


# wrapper around shared/html highlighter.  The shared/hbs highlighter will be
# added into shared/html when a buffer of filetype 'hbs' is actively displayed in the window.
add-highlighter shared/hbs-file regions
add-highlighter shared/hbs-file/html default-region ref html

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden hbs-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden hbs-indent-on-char %[
    evaluate-commands -draft -itersel %[
        # de-indent after closing a yielded block tag
        try %[ execute-keys -draft <space> <a-h> s ^\h+\{\{/([\w-.]+(?:/[\w-.]+)*)\}\}$ <ret> {c\{\{#<c-r>1,\{\{/<c-r>1\}\} <ret> s \A|.\z <ret> 1<a-&> ]
    ]
]

define-command -hidden hbs-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '/' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K/\h* <ret> y j p }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : hbs-filter-around-selections <ret> }
        # indent after lines beginning with : or -
        try %{ execute-keys -draft k <a-x> <a-k> ^\h*[:-] <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

declare-option bool hbs_highlighters_enabled false

define-command -hidden maybe-add-hbs-to-html %{ evaluate-commands %sh{
    if [ "$kak_opt_hbs_highlighters_enabled" == "false" ]; then
        printf %s "
            add-highlighter shared/html/hbs region '\{\{' '\}\}' ref hbs
            add-highlighter shared/html/tag/hbs region '\{\{' '\}\}' ref hbs
            set-option global hbs_highlighters_enabled true
        "
    fi
} }

hook -group hbs-highlight global WinSetOption filetype=hbs %{
    maybe-add-hbs-to-html
    add-highlighter window/hbs-file ref hbs-file
}

hook global WinSetOption filetype=hbs %{
    hook window ModeChange insert:.* -group hbs-hooks  hbs-filter-around-selections
    hook window InsertChar \n -group hbs-indent hbs-indent-on-new-line
    hook window InsertChar .* -group hbs-ident hbs-indent-on-char
    hook window ModeChange insert:.* -group hbs-hooks  html-filter-around-selections
    hook window InsertChar '>' -group hbs-indent html-indent-on-greater-than
    hook window InsertChar \n -group hbs-indent html-indent-on-new-line
}

hook -group hbs-highlight global WinSetOption filetype=(?!hbs).* %{
    remove-highlighter window/hbs-file
}

hook global WinSetOption filetype=(?!hbs).* %{
    remove-hooks window hbs-.+
}
