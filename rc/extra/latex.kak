# https://www.latex-project.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.tex %{
    set buffer filetype latex
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default content latex \
    comment '(?<!\\)%' '\n' ''

add-highlighter -group /latex/comment fill comment
# Scopes, starting with a backslash
add-highlighter -group /latex/content regex '\\(?!_)\w+\b' 0:keyword
# Options passed to scopes, between brackets
add-highlighter -group /latex/content regex '\\(?!_)\w+\b\[([^]]+)\]' 1:value
# Content between dollar signs/pairs
add-highlighter -group /latex/content regex '(?<!\\)\$\$?([^$]|(?<=\\)\$)+\$\$?' 0:magenta
# Emphasized text
add-highlighter -group /latex/content regex '\\(emph|textit)\{([^}]+)\}' 2:default+i
# Bold text
add-highlighter -group /latex/content regex '\\textbf\{([^}]+)\}' 1:default+b

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group latex-highlight global WinSetOption filetype=latex %{ add-highlighter ref latex }

hook -group latex-highlight global WinSetOption filetype=(?!latex).* %{ remove-highlighter latex }
