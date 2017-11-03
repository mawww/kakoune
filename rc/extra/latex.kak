# https://www.latex-project.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.tex %{
    set-option buffer filetype latex
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default content latex \
    comment '(?<!\\)%' '\n' ''

add-highlighter shared/latex/comment fill comment
# Scopes, starting with a backslash
add-highlighter shared/latex/content regex '\\(?!_)\w+\b' 0:keyword
# Options passed to scopes, between brackets
add-highlighter shared/latex/content regex '\\(?!_)\w+\b\[([^\]]+)\]' 1:value
# Content between dollar signs/pairs
add-highlighter shared/latex/content regex '(\$(\\\$|[^$])+\$)|(\$\$(\\\$|[^$])+\$\$)' 0:magenta
# Emphasized text
add-highlighter shared/latex/content regex '\\(emph|textit)\{([^}]+)\}' 2:default+i
# Bold text
add-highlighter shared/latex/content regex '\\textbf\{([^}]+)\}' 1:default+b

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group latex-highlight global WinSetOption filetype=latex %{ add-highlighter window ref latex }

hook -group latex-highlight global WinSetOption filetype=(?!latex).* %{ remove-highlighter window/latex }
