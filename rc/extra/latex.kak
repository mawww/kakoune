# https://www.latex-project.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-tex %{
    set buffer filetype latex
}

hook global BufCreate .*\.tex %{
    set buffer filetype latex
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default content latex \
    comment '^%' '\n' ''

addhl -group /latex/comment fill comment
# Scopes, starting with a backslash
addhl -group /latex/content regex '\\\w+\b' 0:keyword
# Options passed to scopes, between brackets
addhl -group /latex/content regex '\\\w+\b\[([^]]+)\]' 1:value
# Content between dollar signs/pairs
addhl -group /latex/content regex '\$\$?[^$]+\$\$?' 0:magenta
# Emphasized text
addhl -group /latex/content regex '\\(emph|textit)\{([^}]+)\}' 2:default+i
# Bold text
addhl -group /latex/content regex '\\textbf\{([^}]+)\}' 1:default+b

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=latex %{
    addhl ref latex

    set window comment_line_chars '%'
}

hook global WinSetOption filetype=(?!latex).* %{
    rmhl latex
}
