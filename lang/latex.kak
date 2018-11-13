# https://www.latex-project.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.tex %{
    set-option buffer filetype latex
}

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
add-highlighter shared/latex/content/ regex '(\$(\\\$|[^$])+\$)|(\$\$(\\\$|[^$])+\$\$)' 0:magenta
# Emphasized text
add-highlighter shared/latex/content/ regex '\\(emph|textit)\{([^}]+)\}' 2:default+i
# Bold text
add-highlighter shared/latex/content/ regex '\\textbf\{([^}]+)\}' 1:default+b

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group latex-highlight global WinSetOption filetype=latex %{ add-highlighter window/latex ref latex }

hook -group latex-highlight global WinSetOption filetype=(?!latex).* %{ remove-highlighter window/latex }
