# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-makefile %{
    set buffer filetype makefile
}

hook global BufCreate [mM]akefile %{
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

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=makefile %{ addhl ref makefile }
hook global WinSetOption filetype=(?!makefile).* %{ rmhl makefile }
