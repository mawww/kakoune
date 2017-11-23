# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](rst) %{
    set-option buffer filetype restructuredtext
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default content restructuredtext \
    c          \.\.\h*code::\h*c\h*\n          ^\S          '' \
    cabal      \.\.\h*code::\h*cabal\h*\n      ^\S          '' \
    clojure    \.\.\h*code::\h*clojure\h*\n    ^\S          '' \
    coffee     \.\.\h*code::\h*coffee\h*\n     ^\S          '' \
    cpp        \.\.\h*code::\h*cpp\h*\n        ^\S          '' \
    css        \.\.\h*code::\h*css\h*\n        ^\S          '' \
    cucumber   \.\.\h*code::\h*cucumber\h*\n   ^\S          '' \
    d          \.\.\h*code::\h*d\h*\n          ^\S          '' \
    diff       \.\.\h*code::\h*diff\h*\n       ^\S          '' \
    dockerfile \.\.\h*code::\h*dockerfile\h*\n ^\S          '' \
    fish       \.\.\h*code::\h*fish\h*\n       ^\S          '' \
    gas        \.\.\h*code::\h*gas\h*\n        ^\S          '' \
    go         \.\.\h*code::\h*go\h*\n         ^\S          '' \
    haml       \.\.\h*code::\h*haml\h*\n       ^\S          '' \
    haskell    \.\.\h*code::\h*haskell\h*\n    ^\S          '' \
    html       \.\.\h*code::\h*html\h*\n       ^\S          '' \
    ini        \.\.\h*code::\h*ini\h*\n        ^\S          '' \
    java       \.\.\h*code::\h*java\h*\n       ^\S          '' \
    javascript \.\.\h*code::\h*javascript\h*\n ^\S          '' \
    json       \.\.\h*code::\h*json\h*\n       ^\S          '' \
    julia      \.\.\h*code::\h*julia\h*\n      ^\S          '' \
    kak        \.\.\h*code::\h*kak\h*\n        ^\S          '' \
    kickstart  \.\.\h*code::\h*kickstart\h*\n  ^\S          '' \
    latex      \.\.\h*code::\h*latex\h*\n      ^\S          '' \
    lisp       \.\.\h*code::\h*lisp\h*\n       ^\S          '' \
    lua        \.\.\h*code::\h*lua\h*\n        ^\S          '' \
    makefile   \.\.\h*code::\h*makefile\h*\n   ^\S          '' \
    moon       \.\.\h*code::\h*moon\h*\n       ^\S          '' \
    objc       \.\.\h*code::\h*objc\h*\n       ^\S          '' \
    perl       \.\.\h*code::\h*perl\h*\n       ^\S          '' \
    pug        \.\.\h*code::\h*pug\h*\n        ^\S          '' \
    python     \.\.\h*code::\h*python\h*\n     ^\S          '' \
    ragel      \.\.\h*code::\h*ragel\h*\n      ^\S          '' \
    ruby       \.\.\h*code::\h*ruby\h*\n       ^\S          '' \
    rust       \.\.\h*code::\h*rust\h*\n       ^\S          '' \
    sass       \.\.\h*code::\h*sass\h*\n       ^\S          '' \
    scala      \.\.\h*code::\h*scala\h*\n      ^\S          '' \
    scss       \.\.\h*code::\h*scss\h*\n       ^\S          '' \
    sh         \.\.\h*code::\h*sh\h*\n         ^\S          '' \
    swift      \.\.\h*code::\h*swift\h*\n      ^\S          '' \
    tupfile    \.\.\h*code::\h*tupfile\h*\n    ^\S          '' \
    yaml       \.\.\h*code::\h*yaml\h*\n       ^\S          '' \
    code       ::\h*\n             ^[^\s]  ''

add-highlighter shared/restructuredtext/code fill meta

add-highlighter shared/restructuredtext/c          ref c
add-highlighter shared/restructuredtext/cabal      ref cabal
add-highlighter shared/restructuredtext/clojure    ref clojure
add-highlighter shared/restructuredtext/coffee     ref coffee
add-highlighter shared/restructuredtext/cpp        ref cpp
add-highlighter shared/restructuredtext/css        ref css
add-highlighter shared/restructuredtext/cucumber   ref cucumber
add-highlighter shared/restructuredtext/d          ref d
add-highlighter shared/restructuredtext/diff       ref diff
add-highlighter shared/restructuredtext/dockerfile ref dockerfile
add-highlighter shared/restructuredtext/fish       ref fish
add-highlighter shared/restructuredtext/gas        ref gas
add-highlighter shared/restructuredtext/go         ref go
add-highlighter shared/restructuredtext/haml       ref haml
add-highlighter shared/restructuredtext/haskell    ref haskell
add-highlighter shared/restructuredtext/html       ref html
add-highlighter shared/restructuredtext/ini        ref ini
add-highlighter shared/restructuredtext/java       ref java
add-highlighter shared/restructuredtext/javascript ref javascript
add-highlighter shared/restructuredtext/json       ref json
add-highlighter shared/restructuredtext/julia      ref julia
add-highlighter shared/restructuredtext/kak        ref kakrc
add-highlighter shared/restructuredtext/kickstart  ref kickstart
add-highlighter shared/restructuredtext/latex      ref latex
add-highlighter shared/restructuredtext/lisp       ref lisp
add-highlighter shared/restructuredtext/lua        ref lua
add-highlighter shared/restructuredtext/makefile   ref makefile
add-highlighter shared/restructuredtext/moon       ref moon
add-highlighter shared/restructuredtext/objc       ref objc
add-highlighter shared/restructuredtext/perl       ref perl
add-highlighter shared/restructuredtext/pug        ref pug
add-highlighter shared/restructuredtext/python     ref python
add-highlighter shared/restructuredtext/ragel      ref ragel
add-highlighter shared/restructuredtext/ruby       ref ruby
add-highlighter shared/restructuredtext/rust       ref rust
add-highlighter shared/restructuredtext/sass       ref sass
add-highlighter shared/restructuredtext/scala      ref scala
add-highlighter shared/restructuredtext/scss       ref scss
add-highlighter shared/restructuredtext/sh         ref sh
add-highlighter shared/restructuredtext/swift      ref swift
add-highlighter shared/restructuredtext/tupfile    ref tupfile
add-highlighter shared/restructuredtext/yaml       ref yaml

# Setext-style header
# Valid header characters:
# # ! " $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ ` { | } ~

add-highlighter shared/restructuredtext/content regex (\A|\n\n)(#{3,}\n)?[^\n]+\n(#{3,})$ 0:title
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(!{3,}\n)?[^\n]+\n(!{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)("{3,}\n)?[^\n]+\n("{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\${3,}\n)?[^\n]+\n(\${3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(%{3,}\n)?[^\n]+\n(%{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(&{3,}\n)?[^\n]+\n(&{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)('{3,}\n)?[^\n]+\n('{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\({3,}\n)?[^\n]+\n(\({3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\){3,}\n)?[^\n]+\n(\){3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\*{3,}\n)?[^\n]+\n(\*{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\+{3,}\n)?[^\n]+\n(\+{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(,{3,}\n)?[^\n]+\n(,{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(-{3,}\n)?[^\n]+\n(-{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\.{3,}\n)?[^\n]+\n(\.{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(/{3,}\n)?[^\n]+\n(/{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(:{3,}\n)?[^\n]+\n(:{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\;{3,}\n)?[^\n]+\n(\;{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(<{3,}\n)?[^\n]+\n(<{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(={3,}\n)?[^\n]+\n(={3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(>{3,}\n)?[^\n]+\n(>{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\?{3,}\n)?[^\n]+\n(\?{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(@{3,}\n)?[^\n]+\n(@{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\[{3,}\n)?[^\n]+\n(\[{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\\{3,}\n)?[^\n]+\n(\\{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\]{3,}\n)?[^\n]+\n(\]{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\^{3,}\n)?[^\n]+\n(\^{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(_{3,}\n)?[^\n]+\n(_{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(`{3,}\n)?[^\n]+\n(`{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\{{3,}\n)?[^\n]+\n(\{{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\|{3,}\n)?[^\n]+\n(\|{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(\}{3,}\n)?[^\n]+\n(\}{3,})$ 0:header
add-highlighter shared/restructuredtext/content regex (\A|\n\n)(~{3,}\n)?[^\n]+\n(~{3,})$ 0:header

# Inline markup
add-highlighter shared/restructuredtext/content regex [^*](\*\*([^\s*]|([^\s*][^*]*[^\s*]))\*\*)[^*] 1:bold
add-highlighter shared/restructuredtext/content regex [^*](\*([^\s*]|([^\s*][^*]*[^\s*]))\*)[^*] 1:italic
add-highlighter shared/restructuredtext/content regex [^`](``([^\s`]|([^\s`][^`]*[^\s`]))``)[^`] 1:mono

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
#
hook -group restructuredtext-highlight global WinSetOption filetype=restructuredtext %{ add-highlighter window ref restructuredtext }
hook -group restructuredtext-highlight global WinSetOption filetype=(?!restructuredtext).* %{ remove-highlighter window/restructuredtext }
