# https://orgmode.org/ - your life in plain text
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

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
        lisp emacs-lisp scheme lua makefile org moon objc perl pug python ragel ruby rust
        sass scala scss sh swift toml tupfile typescript yaml sql
    "
    for lang in ${languages}; do
        printf "%s\n" "add-highlighter shared/org/${lang} region '#\+(?i)BEGIN_SRC(?I)\h+${lang}\b' '#\+(?i)END_SRC' regions"
        printf "%s\n" "add-highlighter shared/org/${lang}/ default-region fill meta"
        case ${lang} in
            kak)        ref="kakrc"   ;;
            emacs-lisp) ref="lisp"    ;;
            *)          ref="${lang}" ;;
        esac
        printf "%s\n" "add-highlighter shared/org/${lang}/inner region \A#\+(?i)BEGIN_SRC(?I)[^\n]*\K '(?=#\+(?i)END_SRC)' ref ${ref}"
    done
}

# Comment
add-highlighter shared/org/comment region (^|\h)\K#[^+] $ fill comment

# No-language SRC block
add-highlighter shared/org/src_block region ^(\h*)(?i)#\+BEGIN_SRC(\h+|\n) ^(\h*)(?i)#\+END_SRC\h*$ fill meta

# Example block
add-highlighter shared/org/example_block region ^(\h*)(?i)#\+BEGIN_EXAMPLE(\h+|\n) ^(\h*)(?i)#\+END_EXAMPLE\h*$ fill mono

# Small example block
add-highlighter shared/org/inline/text/example regex ^(\h*)[:]\h+[^\n]* 0:mono

# Unordered list items start with `-', `+', or `*'
add-highlighter shared/org/inline/text/unordered-lists regex ^(?:\h*)([-+])\h+ 1:bullet
# But `*' list must be indented with at least single space, if not it is treated as a heading
add-highlighter shared/org/inline/text/star-list regex ^(?:\h+)([*])\h+ 1:bullet

# Ordered list items start with a numeral followed by either a period or a right parenthesis, such as `1.' or `1)'
add-highlighter shared/org/inline/text/ordered-lists regex ^(?:\h*)(\d+[.)])\h+ 1:bullet

# Headings
add-highlighter shared/org/inline/text/heading    regex "^[*]+\h+[^\n]+"                0:header
add-highlighter shared/org/inline/text/italic     regex "\s([/][^\s/].*?[^\s/]*?[/])\W" 1:italic
add-highlighter shared/org/inline/text/verbatim   regex "\s([=][^\s=].*?[^\s=]*?[=])\W" 1:meta
add-highlighter shared/org/inline/text/code       regex "\s([~][^\s~].*?[^\s~]*?[~])\W" 1:mono
add-highlighter shared/org/inline/text/math       regex "\s([$][^\s].*?[^\s]*?[$])\W"   1:mono
add-highlighter shared/org/inline/text/underlined regex "\s([_][^\s_].*?[^\s_]*?[_])\W" 1:underline
add-highlighter shared/org/inline/text/bold       regex "\s([*][^\s*].*?[^\s*]*?[*])\W" 1:bold
add-highlighter shared/org/inline/text/link       regex "\[[^\n]+\]\]"                  0:link
add-highlighter shared/org/inline/text/option     regex "(?i)#\+[a-z]\w*\b"             0:meta

# Strikethrough highlighter is disabled because Kakoune doesn't support strikethrough attribute
# add-highlighter shared/org/inline/text/strikethrough regex "\W*\+[^\s][^\n+]+[^\s]\+\W" 0:default,default+s

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group org-highlight global WinSetOption filetype=org %{
    add-highlighter window/org ref org
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/org }
}

