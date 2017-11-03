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
add-highlighter shared/markdown/yaml       ref yaml

# Setext-style header
add-highlighter shared/markdown/content regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
add-highlighter shared/markdown/content regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
add-highlighter shared/markdown/content regex ^(#+)(\h+)([^\n]+) 1:header

add-highlighter shared/markdown/content regex ^\h?((?:[\s\t]+)?[-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*\n)*$ 0:list 1:bullet
add-highlighter shared/markdown/content regex \B\+[^\n]+?\+\B 0:mono
add-highlighter shared/markdown/content regex \B\*[^\n]+?\*\B 0:italic
add-highlighter shared/markdown/content regex \b_[^\n]+?_\b 0:italic
add-highlighter shared/markdown/content regex \B\*\*[^\n]+?\*\*\B 0:bold
add-highlighter shared/markdown/content regex \B__[^\n]+?__\B 0:bold
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
