# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-makefile %{
    set buffer filetype makefile
}

hook global BufCreate .*/?[mM]akefile %{
    set buffer filetype makefile
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default content makefile \
   comment '#' '$' '' \
   eval '\$\(' '\)' '\('

addhl -group /makefile/comment fill comment
addhl -group /makefile/eval fill value

addhl -group /makefile/content regex ^[\w.%]+\h*:\s 0:identifier
addhl -group /makefile/content regex \b(ifeq|ifneq|else|endif)\b 0:keyword
addhl -group /makefile/content regex [+?:]= 0:operator

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _makefile_fix_whitespaces %{
    try %{
        exec -draft "
            \%
s^[^\s][^:\n]+:[^\n]*\n(^\h+[^\n]+\n?)+<ret>
s {%opt{tabstop}}<ret>
c<tab><esc>
        "
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=makefile %{
    addhl ref makefile

    hook buffer BufWritePre .* -group makefile-indent _makefile_fix_whitespaces

    set window comment_selection_chars ""
    set window comment_line_chars "#"
}

hook global WinSetOption filetype=(?!makefile).* %{
    rmhl makefile
    rmhooks buffer makefile-indent
}
