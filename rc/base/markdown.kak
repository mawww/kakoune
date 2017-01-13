# http://daringfireball.net/projects/markdown
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](markdown|md|mkd) %{
    set buffer filetype markdown
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default content markdown \
    c          ```\h*c          ```          '' \
    cabal      ```\h*cabal      ```          '' \
    clojure    ```\h*clojure    ```          '' \
    coffee     ```\h*coffee     ```          '' \
    cpp        ```\h*cpp        ```          '' \
    css        ```\h*css        ```          '' \
    cucumber   ```\h*cucumber   ```          '' \
    d          ```\h*d          ```          '' \
    diff       ```\h*diff       ```          '' \
    dockerfile ```\h*dockerfile ```          '' \
    fish       ```\h*fish       ```          '' \
    gas        ```\h*gas        ```          '' \
    go         ```\h*go         ```          '' \
    haml       ```\h*haml       ```          '' \
    haskell    ```\h*haskell    ```          '' \
    html       ```\h*html       ```          '' \
    ini        ```\h*ini        ```          '' \
    java       ```\h*java       ```          '' \
    javascript ```\h*javascript ```          '' \
    json       ```\h*json       ```          '' \
    julia      ```\h*julia      ```          '' \
    kak        ```\h*kak        ```          '' \
    kickstart  ```\h*kickstart  ```          '' \
    latex      ```\h*latex      ```          '' \
    lisp       ```\h*lisp       ```          '' \
    lua        ```\h*lua        ```          '' \
    makefile   ```\h*makefile   ```          '' \
    moon       ```\h*moon       ```          '' \
    objc       ```\h*objc       ```          '' \
    perl       ```\h*perl       ```          '' \
    pug        ```\h*pug        ```          '' \
    python     ```\h*python     ```          '' \
    ragel      ```\h*ragel      ```          '' \
    ruby       ```\h*ruby       ```          '' \
    rust       ```\h*rust       ```          '' \
    sass       ```\h*sass       ```          '' \
    scala      ```\h*scala      ```          '' \
    scss       ```\h*scss       ```          '' \
    sh         ```\h*sh         ```          '' \
    swift      ```\h*swift      ```          '' \
    tupfile    ```\h*tupfile    ```          '' \
    yaml       ```\h*yaml       ```          '' \
    code       ```              ```          '' \
    code       ``               ``           '' \
    code       `                `            ''

add-highlighter -group /markdown/code fill meta

add-highlighter -group /markdown/c          ref c
add-highlighter -group /markdown/cabal      ref cabal
add-highlighter -group /markdown/clojure    ref clojure
add-highlighter -group /markdown/coffee     ref coffee
add-highlighter -group /markdown/cpp        ref cpp
add-highlighter -group /markdown/css        ref css
add-highlighter -group /markdown/cucumber   ref cucumber
add-highlighter -group /markdown/d          ref d
add-highlighter -group /markdown/diff       ref diff
add-highlighter -group /markdown/dockerfile ref dockerfile
add-highlighter -group /markdown/fish       ref fish
add-highlighter -group /markdown/gas        ref gas
add-highlighter -group /markdown/go         ref go
add-highlighter -group /markdown/haml       ref haml
add-highlighter -group /markdown/haskell    ref haskell
add-highlighter -group /markdown/html       ref html
add-highlighter -group /markdown/ini        ref ini
add-highlighter -group /markdown/java       ref java
add-highlighter -group /markdown/javascript ref javascript
add-highlighter -group /markdown/json       ref json
add-highlighter -group /markdown/julia      ref julia
add-highlighter -group /markdown/kak        ref kak
add-highlighter -group /markdown/kickstart  ref kickstart
add-highlighter -group /markdown/latex      ref latex
add-highlighter -group /markdown/lisp       ref lisp
add-highlighter -group /markdown/lua        ref lua
add-highlighter -group /markdown/makefile   ref makefile
add-highlighter -group /markdown/moon       ref moon
add-highlighter -group /markdown/objc       ref objc
add-highlighter -group /markdown/perl       ref perl
add-highlighter -group /markdown/pug        ref pug
add-highlighter -group /markdown/python     ref python
add-highlighter -group /markdown/ragel      ref ragel
add-highlighter -group /markdown/ruby       ref ruby
add-highlighter -group /markdown/rust       ref rust
add-highlighter -group /markdown/sass       ref sass
add-highlighter -group /markdown/scala      ref scala
add-highlighter -group /markdown/scss       ref scss
add-highlighter -group /markdown/sh         ref sh
add-highlighter -group /markdown/swift      ref swift
add-highlighter -group /markdown/tupfile    ref tupfile
add-highlighter -group /markdown/yaml       ref yaml

# Setext-style header
add-highlighter -group /markdown/content regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
add-highlighter -group /markdown/content regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
add-highlighter -group /markdown/content regex ^(#+)(\h+)([^\n]+) 1:header

add-highlighter -group /markdown/content regex ^\h?+((?:[\s\t]+)?[-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*\n)*$ 0:list 1:bullet
add-highlighter -group /markdown/content regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:block
add-highlighter -group /markdown/content regex \B\+[^\n]+?\+\B 0:mono
add-highlighter -group /markdown/content regex \B\*[^\n]+?\*\B 0:italic
add-highlighter -group /markdown/content regex \b_[^\n]+?_\b 0:italic
add-highlighter -group /markdown/content regex \B\*\*[^\n]+?\*\*\B 0:bold
add-highlighter -group /markdown/content regex \B__[^\n]+?__\B 0:bold
add-highlighter -group /markdown/content regex <(([a-z]+://.*?)|((mailto:)?[\w+-]+@[a-z]+[.][a-z]+))> 0:link
add-highlighter -group /markdown/content regex ^\h*(>\h*)+ 0:comment
add-highlighter -group /markdown/content regex \H\K\h\h$ 0:PrimarySelection

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden markdown-indent-on-new-line %{
    eval -draft -itersel %{
        # copy block quote(s), list item prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K((>\h*)|[*+-])+\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # remove trailing white spaces
        try %{ exec -draft -itersel %{ k<a-x> s \h+$ <ret> d } }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group markdown-highlight global WinSetOption filetype=markdown %{ add-highlighter ref markdown }

hook global WinSetOption filetype=markdown %{
    hook window InsertChar \n -group markdown-indent markdown-indent-on-new-line
}

hook -group markdown-highlight global WinSetOption filetype=(?!markdown).* %{ remove-highlighter markdown }

hook global WinSetOption filetype=(?!markdown).* %{
    remove-hooks window markdown-indent
}
