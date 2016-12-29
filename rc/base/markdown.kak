# http://daringfireball.net/projects/markdown
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](markdown|md|mkd) %{
    set buffer filetype markdown
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default content markdown \
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

addhl -group /markdown/code fill meta

addhl -group /markdown/c          ref c
addhl -group /markdown/cabal      ref cabal
addhl -group /markdown/clojure    ref clojure
addhl -group /markdown/coffee     ref coffee
addhl -group /markdown/cpp        ref cpp
addhl -group /markdown/css        ref css
addhl -group /markdown/cucumber   ref cucumber
addhl -group /markdown/d          ref d
addhl -group /markdown/diff       ref diff
addhl -group /markdown/dockerfile ref dockerfile
addhl -group /markdown/fish       ref fish
addhl -group /markdown/gas        ref gas
addhl -group /markdown/go         ref go
addhl -group /markdown/haml       ref haml
addhl -group /markdown/haskell    ref haskell
addhl -group /markdown/html       ref html
addhl -group /markdown/ini        ref ini
addhl -group /markdown/java       ref java
addhl -group /markdown/javascript ref javascript
addhl -group /markdown/json       ref json
addhl -group /markdown/julia      ref julia
addhl -group /markdown/kak        ref kak
addhl -group /markdown/kickstart  ref kickstart
addhl -group /markdown/latex      ref latex
addhl -group /markdown/lisp       ref lisp
addhl -group /markdown/lua        ref lua
addhl -group /markdown/makefile   ref makefile
addhl -group /markdown/moon       ref moon
addhl -group /markdown/objc       ref objc
addhl -group /markdown/perl       ref perl
addhl -group /markdown/pug        ref pug
addhl -group /markdown/python     ref python
addhl -group /markdown/ragel      ref ragel
addhl -group /markdown/ruby       ref ruby
addhl -group /markdown/rust       ref rust
addhl -group /markdown/sass       ref sass
addhl -group /markdown/scala      ref scala
addhl -group /markdown/scss       ref scss
addhl -group /markdown/sh         ref sh
addhl -group /markdown/swift      ref swift
addhl -group /markdown/tupfile    ref tupfile
addhl -group /markdown/yaml       ref yaml

# Setext-style header
addhl -group /markdown/content regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
addhl -group /markdown/content regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
addhl -group /markdown/content regex ^(#+)(\h+)([^\n]+) 1:header

addhl -group /markdown/content regex ^\h?+((?:[\s\t]+)?[-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*\n)*$ 0:list 1:bullet
addhl -group /markdown/content regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:block
addhl -group /markdown/content regex \B\+[^\n]+?\+\B 0:mono
addhl -group /markdown/content regex \B\*[^\n]+?\*\B 0:italic
addhl -group /markdown/content regex \b_[^\n]+?_\b 0:italic
addhl -group /markdown/content regex \B\*\*[^\n]+?\*\*\B 0:bold
addhl -group /markdown/content regex \B__[^\n]+?__\B 0:bold
addhl -group /markdown/content regex <(([a-z]+://.*?)|((mailto:)?[\w+-]+@[a-z]+[.][a-z]+))> 0:link
addhl -group /markdown/content regex ^\h*(>\h*)+ 0:comment
addhl -group /markdown/content regex \H\K\h\h$ 0:PrimarySelection

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _markdown_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # remove trailing white spaces
        try %{ exec -draft -itersel %{ k<a-x> s \h+$ <ret> d } }
        # copy block quote(s), list item prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K((>\h*)|[*+-])+\h* <ret> y gh j P }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group markdown-highlight global WinSetOption filetype=markdown %{ addhl ref markdown }

hook global WinSetOption filetype=markdown %{
    hook window InsertChar \n -group markdown-indent _markdown_indent_on_new_line
}

hook -group markdown-highlight global WinSetOption filetype=(?!markdown).* %{ rmhl markdown }

hook global WinSetOption filetype=(?!markdown).* %{
    rmhooks window markdown-indent
}
