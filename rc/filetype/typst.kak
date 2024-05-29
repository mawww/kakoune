# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](typ) %{
    set-option buffer filetype typst
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group typst-highlight global WinSetOption filetype=typst %{
    require-module typst

    add-highlighter window/typst ref typst
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/typst }
    hook window InsertChar \n -group typst typst-on-new-line
}

provide-module typst %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/typst group

# Comments
add-highlighter shared/typst/ regex ^//(?:[^\n/][^\n]*|)$ 0:comment

# Strings
add-highlighter shared/typst/ regex '"[^"]*"' 0:string

# Headings
add-highlighter shared/typst/ regex ^=+\h+[^\n]+$ 0:header

# Code blocks
# Raw with optional syntax highlighting
add-highlighter shared/typst/ regex '^```[^(```)]*```' 0:mono
# Multiline monospace
add-highlighter shared/typst/ regex '^`[^(`)]*`' 0:mono

# Monospace text
add-highlighter shared/typst/ regex \B(`[^\n]+?`)\B 0:mono
add-highlighter shared/typst/ regex \B(```[^\n]+?```)\B 0:mono

# Bold text
add-highlighter shared/typst/ regex \s\*[^\*]+\*\B 0:+b

# Italic text
add-highlighter shared/typst/ regex \b_.*?_\b 0:+i

# Code expressions: functions, variables
add-highlighter shared/typst/ regex (^|\h)#(\w|\.|-)+ 0:meta

# Bold terms in term lists
add-highlighter shared/typst/ regex ^/\h[^:]*: 0:+b

§

# Commands
# ‾‾‾‾‾‾‾‾
 
define-command -hidden typst-on-new-line %<
    evaluate-commands -draft -itersel %<
        # Preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # Cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k x s \h+$ <ret> d }
    >
>
