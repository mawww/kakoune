# http://daringfireball.net/projects/markdown
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](markdown|md|mkd) %{
    set-option buffer filetype markdown
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default content markdown \
    c          ^```\h*c\b\K          ```          '' \
    cabal      ^```\h*cabal\b\K      ```          '' \
    clojure    ^```\h*clojure\b\K    ```          '' \
    coffee     ^```\h*coffee\b\K     ```          '' \
    cpp        ^```\h*cpp\b\K        ```          '' \
    css        ^```\h*css\b\K        ```          '' \
    cucumber   ^```\h*cucumber\b\K   ```          '' \
    d          ^```\h*d\b\K          ```          '' \
    diff       ^```\h*diff\b\K       ```          '' \
    dockerfile ^```\h*dockerfile\b\K ```          '' \
    fish       ^```\h*fish\b\K       ```          '' \
    gas        ^```\h*gas\b\K        ```          '' \
    go         ^```\h*go\b\K         ```          '' \
    haml       ^```\h*haml\b\K       ```          '' \
    haskell    ^```\h*haskell\b\K    ```          '' \
    html       ^```\h*html\b\K       ```          '' \
    ini        ^```\h*ini\b\K        ```          '' \
    java       ^```\h*java\b\K       ```          '' \
    javascript ^```\h*javascript\b\K ```          '' \
    json       ^```\h*json\b\K       ```          '' \
    julia      ^```\h*julia\b\K      ```          '' \
    kak        ^```\h*kak\b\K        ```          '' \
    kickstart  ^```\h*kickstart\b\K  ```          '' \
    latex      ^```\h*latex\b\K      ```          '' \
    lisp       ^```\h*lisp\b\K       ```          '' \
    lua        ^```\h*lua\b\K        ```          '' \
    makefile   ^```\h*makefile\b\K   ```          '' \
    moon       ^```\h*moon\b\K       ```          '' \
    objc       ^```\h*objc\b\K       ```          '' \
    perl       ^```\h*perl\b\K       ```          '' \
    pug        ^```\h*pug\b\K        ```          '' \
    python     ^```\h*python\b\K     ```          '' \
    ragel      ^```\h*ragel\b\K      ```          '' \
    ruby       ^```\h*ruby\b\K       ```          '' \
    rust       ^```\h*rust\b\K       ```          '' \
    sass       ^```\h*sass\b\K       ```          '' \
    scala      ^```\h*scala\b\K      ```          '' \
    scss       ^```\h*scss\b\K       ```          '' \
    sh         ^```\h*sh\b\K         ```          '' \
    swift      ^```\h*swift\b\K      ```          '' \
    tupfile    ^```\h*tupfile\b\K    ```          '' \
    typescript ^```\h*typescript\b\K ```          '' \
    yaml       ^```\h*yaml\b\K       ```          '' \
    code       ^```((!?=(c|cabal|clojure|coffee|cpp|css|cucumber|diff|dockerfile|fish|gas|go|haml|haskell|html|ini|java|javascript|json|julia|kakrc|kickstart|latex|lisp|lua|makefile|moon|objc|perl|pug|python|ragel|ruby|rust|sass|scala|scss|sh|swift|tupfile|typescript|yaml)[^\n])*)$          ```          '' \
    code       ^``[^`]            ``           '' \
    code       ^`[^`]             `            ''

add-highlighter shared/markdown/code fill meta

add-highlighter shared/markdown/c          ref c
add-highlighter shared/markdown/cabal      ref cabal
add-highlighter shared/markdown/clojure    ref clojure
add-highlighter shared/markdown/coffee     ref coffee
add-highlighter shared/markdown/cpp        ref cpp
add-highlighter shared/markdown/css        ref css
add-highlighter shared/markdown/cucumber   ref cucumber
add-highlighter shared/markdown/d          ref d
add-highlighter shared/markdown/diff       ref diff
add-highlighter shared/markdown/dockerfile ref dockerfile
add-highlighter shared/markdown/fish       ref fish
add-highlighter shared/markdown/gas        ref gas
add-highlighter shared/markdown/go         ref go
add-highlighter shared/markdown/haml       ref haml
add-highlighter shared/markdown/haskell    ref haskell
add-highlighter shared/markdown/html       ref html
add-highlighter shared/markdown/ini        ref ini
add-highlighter shared/markdown/java       ref java
add-highlighter shared/markdown/javascript ref javascript
add-highlighter shared/markdown/json       ref json
add-highlighter shared/markdown/julia      ref julia
add-highlighter shared/markdown/kak        ref kakrc
add-highlighter shared/markdown/kickstart  ref kickstart
add-highlighter shared/markdown/latex      ref latex
add-highlighter shared/markdown/lisp       ref lisp
add-highlighter shared/markdown/lua        ref lua
add-highlighter shared/markdown/makefile   ref makefile
add-highlighter shared/markdown/moon       ref moon
add-highlighter shared/markdown/objc       ref objc
add-highlighter shared/markdown/perl       ref perl
add-highlighter shared/markdown/pug        ref pug
add-highlighter shared/markdown/python     ref python
add-highlighter shared/markdown/ragel      ref ragel
add-highlighter shared/markdown/ruby       ref ruby
add-highlighter shared/markdown/rust       ref rust
add-highlighter shared/markdown/sass       ref sass
add-highlighter shared/markdown/scala      ref scala
add-highlighter shared/markdown/scss       ref scss
add-highlighter shared/markdown/sh         ref sh
add-highlighter shared/markdown/swift      ref swift
add-highlighter shared/markdown/tupfile    ref tupfile
add-highlighter shared/markdown/typescript ref typescript
add-highlighter shared/markdown/yaml       ref yaml

# Setext-style header
add-highlighter shared/markdown/content regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
add-highlighter shared/markdown/content regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
add-highlighter shared/markdown/content regex ^(#+)(\h+)([^\n]+) 1:header

add-highlighter shared/markdown/content regex ^\h?((?:[\s\t]+)?[-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*\n)*$ 0:list 1:bullet
add-highlighter shared/markdown/content regex \B\+[^\n]+?\+\B 0:mono
add-highlighter shared/markdown/content regex [^`](``([^\s`]|([^\s`][^`]*[^\s`]))``)[^`] 1:mono
add-highlighter shared/markdown/content regex [^*](\*([^\s*]|([^\s*][^*]*[^\s*]))\*)[^*] 1:italic
add-highlighter shared/markdown/content regex [^_](_([^\s_]|([^\s_][^_]*[^\s_]))_)[^_] 1:italic
add-highlighter shared/markdown/content regex [^*](\*\*([^\s*]|([^\s*][^*]*[^\s*]))\*\*)[^*] 1:bold
add-highlighter shared/markdown/content regex [^_](__([^\s_]|([^\s_][^_]*[^\s_]))__)[^_] 1:bold
add-highlighter shared/markdown/content regex <(([a-z]+://.*?)|((mailto:)?[\w+-]+@[a-z]+[.][a-z]+))> 0:link
add-highlighter shared/markdown/content regex ^\h*(>\h*)+ 0:comment
add-highlighter shared/markdown/content regex \H\K\h\h$ 0:PrimarySelection

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

hook -group markdown-highlight global WinSetOption filetype=markdown %{ add-highlighter window ref markdown }

hook global WinSetOption filetype=markdown %{
    hook window InsertChar \n -group markdown-indent markdown-indent-on-new-line
}

hook -group markdown-highlight global WinSetOption filetype=(?!markdown).* %{ remove-highlighter window/markdown }

hook global WinSetOption filetype=(?!markdown).* %{
    remove-hooks window markdown-indent
}
