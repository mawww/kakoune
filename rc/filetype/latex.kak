# https://www.latex-project.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(tex|cls|sty|dtx) %{
    set-option buffer filetype latex
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=latex %(
    require-module latex

    hook window InsertChar \n -group latex-indent %{ latex-indent-newline }
    hook window InsertChar \} -group latex-indent %{ latex-indent-closing-brace }
    hook window ModeChange pop:insert:.* -group latex-indent %{ latex-trim-indent }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks latex-indent }
)

hook -group latex-highlight global WinSetOption filetype=latex %{
    add-highlighter window/latex ref latex
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/latex }
}

provide-module latex %~

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/latex regions
add-highlighter shared/latex/content default-region group
add-highlighter shared/latex/comment region '(?<!\\)%' '\n' fill comment

# Scopes, starting with a backslash
add-highlighter shared/latex/content/ regex '\\(?!_)\w+\b' 0:keyword
# Options passed to scopes, between brackets
add-highlighter shared/latex/content/ regex '\\(?!_)\w+\b\[([^\]]+)\]' 1:value
# Content between dollar signs/pairs
add-highlighter shared/latex/content/ regex '((?<!\\)\$(\\\$|[^$])+\$)|((?<!\\)\$\$(\\\$|[^$])+\$\$)|((?<!\\)\\\[.*?\\\])|(\\\(.*?\\\))' 0:meta
# Emphasized text
add-highlighter shared/latex/content/ regex '\\(emph|textit)\{([^}]+)\}' 2:default+i
# Bold text
add-highlighter shared/latex/content/ regex '\\textbf\{([^}]+)\}' 1:default+b
# Section headings
add-highlighter shared/latex/content/ regex '\\(part|section)\*?\{([^}]+)\}' 2:title
add-highlighter shared/latex/content/ regex '\\(chapter|(sub)+section|(sub)*paragraph)\*?\{([^}]+)\}' 4:header


# Indent
# ------

define-command -hidden latex-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        try %{ execute-keys <a-x> 1s^(\h+)$<ret> d }
    }
}

define-command -hidden latex-indent-newline %(
    evaluate-commands -no-hooks -draft -itersel %(
        # copy '%' comment prefix and following white spaces
        try %{ execute-keys -draft k<a-x> s^\h*%\h*<ret> y jgh P }
        # preserve previous line indent
        try %{ execute-keys -draft K<a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k<a-x> s\h+$<ret> d }
        # indent after line ending with {
        try %( execute-keys -draft k<a-x> <a-k>\{$<ret> j<a-gt> )
        # deindent closing brace(s) when after cursor
        try %( execute-keys -draft <a-x> <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> )
        # indent after line ending with \begin{...}[...]{...}, with multiple
        # sets of arguments possible
        try %(
            execute-keys -draft \
                k<a-x> \
                <a-k>\\begin\h*\{[^\}]+\}(\h|\[.*\]|\{.*\})*$<ret> \
                j<a-gt>
        )
    )
)

define-command -hidden latex-indent-closing-brace %(
    evaluate-commands -no-hooks -draft -itersel %(
        # Align lone } with matching bracket
        try %( execute-keys -draft <a-x>_ <a-k>\A\}\z<ret> m<a-S>1<a-&> )
        # Align \end{...} with corresponding \begin{...}
        try %(
            execute-keys -draft h<a-h> 1s\\end\h*\{([^\}]+)\}\z<ret> \
                <a-?>\\begin\s*\{<c-r>.\}<ret> <a-S>1<a-&>
        )
    )
)

~
