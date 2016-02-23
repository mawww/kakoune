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
        ## If the line above is a target or begins with a tab, then add another tab
        exec -draft <esc> K <a-k>^(\S[^:\v]*:[^:=]|\t\S)<ret>
        exec <tab>
    } catch %{
        ## If we hit the return key twice while implementing a target,
        ## we remove the tab character left alone on the previous line
        try %{ exec -draft <esc> K s^\t$<ret>d }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=makefile %{
    addhl ref makefile

    hook window InsertChar \n -group makefile-indent _makefile_fix_whitespaces

    set window comment_selection_chars ""
    set window comment_line_chars "#"
}

hook global WinSetOption filetype=(?!makefile).* %{
    rmhl makefile
    rmhooks window makefile-indent
}
