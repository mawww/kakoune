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

#+BEGIN_SRC c

#+END_SRC
evaluate-commands %sh{
  languages="
    c cabal clojure coffee cpp css cucumber d diff dockerfile fish gas go
    haml haskell html ini java javascript json julia kak kickstart latex
    lisp lua makefile org moon objc perl pug python ragel ruby rust
    sass scala scss sh swift toml tupfile typescript yaml sql
  "
  for lang in ${languages}; do
    printf 'add-highlighter shared/org/%s region ^(\h*)#\+(?i)BEGIN_SRC(?I)\h+%s\b ^(\h*)#\+(?i)END_SRC regions\n' "${lang}" "${lang}"
    printf 'add-highlighter shared/org/%s/ default-region fill meta\n' "${lang}"
  done
}

add-highlighter shared/org/codeblock region ^(\h*)(?i)#\+BEGIN_EXAMPLE\h* ^(\h*)(?i)#\+END_EXAMPLE\h*$ fill meta

add-highlighter shared/org/listblock region ^\h*[-*]\s ^\h*((?=[-*])|$) regions
add-highlighter shared/org/listblock/marker region \A [-*]\s fill bullet
add-highlighter shared/org/listblock/content default-region ref org/inline

# Setext-style header
# add-highlighter shared/org/inline/text/ regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
# add-highlighter shared/org/inline/text/ regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
# add-highlighter shared/org/inline/text/ regex ^#[^\n]* 0:header

add-highlighter shared/org/inline/text/intalic regex \W*/([^\s].+[^\s])/\W 1:default,default+i
add-highlighter shared/org/inline/text/verbatim regex \W*=([^\s].+[^\s])=\W 1:meta
add-highlighter shared/org/inline/text/code regex \W*~([^\s].+[^\s])~\W 1:mono
# add-highlighter shared/org/inline/text/ regex \s*\+[^\s][^\n+]+[^\s]\+\s 0:default,default+s
add-highlighter shared/org/inline/text/underlined regex \W*_([^\s].+[^\s])_\W 1:default,default+u
add-highlighter shared/org/inline/text/bold regex \W*\*([^\s].+[^\s])\*\W 1:default,default+b
add-highlighter shared/org/inline/text/ regex \[[^\n]+\]\] 0:link
# add-highlighter shared/org/inline/text/ regex ^\h*(>\h*)+ 0:comment

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group org-highlight global WinSetOption filetype=org %{
    add-highlighter window/org ref org
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/org }
}

