# http://daringfireball.net/projects/org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.]org %{
    set-option buffer filetype org
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/org regions
add-highlighter shared/org/inline default-region regions
add-highlighter shared/org/inline/text default-region group

evaluate-commands %sh{
    languages="
        c cabal clojure coffee cpp css cucumber d diff dockerfile fish gas go
        haml haskell html ini java javascript json julia kak kickstart latex
        lisp lua makefile org moon objc perl pug python ragel ruby rust
        sass scala scss sh swift toml tupfile typescript yaml sql
    "
    for lang in ${languages}; do
        printf "%s\n" "add-highlighter shared/org/${lang} region '#\\+BEGIN_SRC\\h+${lang}\\b' '#\\+END_SRC' regions"
        printf "%s\n" "add-highlighter shared/org/${lang}/ default-region fill meta"
        [ "${lang}" = kak ] && ref=kakrc || ref="${lang}"
        printf "%s\n" "add-highlighter shared/org/${lang}/inner region \\A#\\+BEGIN_SRC[^\n]*\\K '(?=#\\+END_SRC)' ref ${ref}"
    done
}

add-highlighter shared/org/codeblock region ^(\h*)(?i)#\+BEGIN_SRC(\h+|\n) ^(\h*)(?i)#\+END_SRC\h*$ fill meta
add-highlighter shared/org/exampleblock region ^(\h*)(?i)#\+BEGIN_EXAMPLE(\h+|\n) ^(\h*)(?i)#\+END_EXAMPLE\h*$ fill meta

# Unordered list items start with `-', `+', or `*'
add-highlighter shared/org/inline/text/unordered-lists regex ^(?:\h*)([-+]) 1:bullet
add-highlighter shared/org/inline/text/star-list regex ^(?:\h+)([*]) 1:bullet

# Ordered list items start with a numeral followed by either a period or a right parenthesis, such as `1.' or `1)'
add-highlighter shared/org/inline/text/ordered-lists regex ^(?:\h*)(\d+[.)]) 1:bullet

# Headings
add-highlighter shared/org/inline/text/ regex ^[*]+\h+[^\n]* 0:header

add-highlighter shared/org/inline/text/intalic regex \W*/([^\s].+[^\s])/\W 1:default,default+i
add-highlighter shared/org/inline/text/verbatim regex \W*=([^\s].+[^\s])=\W 1:meta
add-highlighter shared/org/inline/text/code regex \W*~([^\s].+[^\s])~\W 1:mono
# add-highlighter shared/org/inline/text/ regex \s*\+[^\s][^\n+]+[^\s]\+\s 0:default,default+s
add-highlighter shared/org/inline/text/underlined regex \W*_([^\s].+[^\s])_\W 1:default,default+u
add-highlighter shared/org/inline/text/bold regex \W*\*([^\s*].+[^\s])\*\W 1:default,default+b
add-highlighter shared/org/inline/text/ regex \[[^\n]+\]\] 0:link
# add-highlighter shared/org/inline/text/ regex ^\h*(>\h*)+ 0:comment

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group org-highlight global WinSetOption filetype=org %{
    add-highlighter window/org ref org
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/org }
}

