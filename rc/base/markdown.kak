# http://daringfireball.net/projects/markdown
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](markdown|md|mkd) %{
    set-option buffer filetype markdown
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/markdown regions
add-highlighter shared/markdown/content default-region group
add-highlighter shared/markdown/code region ``` ``` fill meta

evaluate-commands %sh{
  languages="
    c cabal clojure coffee cpp css cucumber d diff dockerfile fish gas go
    haml haskell html ini java javascript json julia kak kickstart latex
    lisp lua makefile markdown moon objc perl pug python ragel ruby rust
    sass scala scss sh swift tupfile typescript yaml
  "
  for lang in ${languages}; do
    printf 'add-highlighter shared/markdown/%s region ```\h*%s\\b   ``` regions\n' "${lang}" "${lang}"
    printf 'add-highlighter shared/markdown/%s/ default-region fill meta\n' "${lang}"
    [ "${lang}" = kak ] && ref=kakrc || ref="${lang}"
    printf 'add-highlighter shared/markdown/%s/inner region \A```[^\\n]*\K (?=```) ref %s\n' "${lang}" "${ref}"
  done
}

# Setext-style header
add-highlighter shared/markdown/content/ regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
add-highlighter shared/markdown/content/ regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
add-highlighter shared/markdown/content/ regex ^(#+)(\h+)([^\n]+) 1:header

add-highlighter shared/markdown/content/ regex ^\h?((?:[\s\t]+)?[-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*\n)*$ 0:list 1:bullet
add-highlighter shared/markdown/content/ regex \B\+[^\n]+?\+\B 0:mono
add-highlighter shared/markdown/content/ regex [^`](`([^\s`]|([^\s`](\n?[^\n`])*[^\s`]))`)[^`] 1:mono
add-highlighter shared/markdown/content/ regex [^`](``([^\s`]|([^\s`](\n?[^\n`])*[^\s`]))``)[^`] 1:mono
add-highlighter shared/markdown/content/ regex [^*](\*([^\s*]|([^\s*](\n?[^\n*])*[^\s*]))\*)[^*] 1:italic
add-highlighter shared/markdown/content/ regex [^_](_([^\s_]|([^\s_](\n?[^\n_])*[^\s_]))_)[^_] 1:italic
add-highlighter shared/markdown/content/ regex [^*](\*\*([^\s*]|([^\s*](\n?[^\n*])*[^\s*]))\*\*)[^*] 1:bold
add-highlighter shared/markdown/content/ regex [^_](__([^\s_]|([^\s_](\n?[^\n_])*[^\s_]))__)[^_] 1:bold
add-highlighter shared/markdown/content/ regex <(([a-z]+://.*?)|((mailto:)?[\w+-]+@[a-z]+[.][a-z]+))> 0:link
add-highlighter shared/markdown/content/ regex ^\[[^\]\n]*\]:\h*([^\n]*) 1:link
add-highlighter shared/markdown/content/ regex ^\h*(>\h*)+ 0:comment
add-highlighter shared/markdown/content/ regex \H\K\h\h$ 0:PrimarySelection

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden markdown-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy block quote(s), list item prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K((>\h*)+([*+-]\h)?|(>\h*)*[*+-]\h)\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # remove trailing white spaces
        try %{ execute-keys -draft -itersel %{ k<a-x> s \h+$ <ret> d } }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group markdown-highlight global WinSetOption filetype=markdown %{ add-highlighter window/markdown ref markdown }

hook global WinSetOption filetype=markdown %{
    hook window InsertChar \n -group markdown-indent markdown-indent-on-new-line
}

hook -group markdown-highlight global WinSetOption filetype=(?!markdown).* %{ remove-highlighter window/markdown }

hook global WinSetOption filetype=(?!markdown).* %{
    remove-hooks window markdown-indent
}
