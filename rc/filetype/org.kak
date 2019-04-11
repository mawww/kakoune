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
    languages="arch-linux asciidoc cabal c cpp objc clojure
               cmake coffee css cucumber dart diff d dockerfile
               elixir elm etc exherbo fish gas git go haml haskell
               hbs html i3 ini java javascript json julia justrc
               kickstart latex emacs-lisp lisp lua mail makefile markdown
               mercurial moon nim ocaml perl php pony protobuf
               pug python ragel restructuredtext ruby rust sass
               scala scheme scss sh sql swift systemd taskpaper
               toml troff tupfile void-linux yaml"

    for lang in ${languages}; do
        printf "%s\n" "add-highlighter shared/org/${lang} region '#\+(?i)BEGIN_SRC(?I)\h+${lang}\b' '(?i)#\+END_SRC' regions"
        printf "%s\n" "add-highlighter shared/org/${lang}/ default-region fill meta"
        case ${lang} in
            # here we need to declare all workarounds for Org-mode supported languages
            kak)        ref="kakrc"   ;;
            emacs-lisp) ref="lisp"    ;;
            *)          ref="${lang}" ;;
        esac
        printf "%s\n" "add-highlighter shared/org/${lang}/inner region \A#\+(?i)BEGIN_SRC(?I)\h+[^\n]+\K '(?i)(?=#\+END_SRC)' ref ${ref}"
    done
}

# No-language SRC block
add-highlighter shared/org/src_block region ^(\h*)(?i)#\+BEGIN_SRC(\h+|\n) ^(\h*)(?i)#\+END_SRC\h*$ fill meta

# Comment
add-highlighter shared/org/comment region (^|\h)\K#[^+] $ fill comment

# Table
add-highlighter shared/org/table region ^\h*[|][^+] $ fill string

# Various blocks
evaluate-commands %sh{
    blocks="EXAMPLE QUOTE EXPORT CENTER VERSE"
    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }
    printf "%s\n" "add-highlighter shared/org/block region -match-capture ^\h*(?i)#\+BEGIN_($(join "${blocks}" '|'))(\h+|\n) ^\h*(?i)#\+END_($(join "${blocks}" '|'))\h*$ fill meta"
}

# Small example block
add-highlighter shared/org/inline/text/example regex ^(\h*)[:]\h+[^\n]* 0:meta

# Unordered list items start with `-', `+', or `*'
add-highlighter shared/org/inline/text/unordered-lists regex ^(?:\h*)([-+])\h+ 1:bullet

# But `*' list must be indented with at least single space, if not it is treated as a heading
add-highlighter shared/org/inline/text/star-list regex ^(?:\h+)([*])\h+ 1:bullet

# Ordered list items start with a numeral followed by either a period or a right parenthesis, such as `1.' or `1)'
add-highlighter shared/org/inline/text/ordered-lists regex ^(?:\h*)(\d+[.)])\h+ 1:bullet

# Headings
add-highlighter shared/org/inline/text/heading       regex "^([*]{1}|[*]{5})\h+[^\n]+"  0:header
add-highlighter shared/org/inline/text/section       regex "^([*]{2}|[*]{6})\h+[^\n]+"  0:section
add-highlighter shared/org/inline/text/subsection    regex "^([*]{3}|[*]{7})\h+[^\n]+"  0:subsection
add-highlighter shared/org/inline/text/subsubsection regex "^([*]{4}|[*]{8,})\h+[^\n]+" 0:subsubsection

# Options
add-highlighter shared/org/inline/text/option     regex "(?i)#\+[a-z]\w*\b[^\n]*"          0:module
add-highlighter shared/org/inline/text/title      regex "(?i)#\+title:([^\n]+)"            0:module 1:title
add-highlighter shared/org/inline/text/requisites regex "(?i)#\+(?:author|email):([^\n]+)" 0:module 1:keyword

# Markup
add-highlighter shared/org/inline/text/italic        regex "(^|\h)([/]\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[/])\W"        2:italic
add-highlighter shared/org/inline/text/strikethrough regex "(^|\h)([+]\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[+])\W"        2:strikethrough
add-highlighter shared/org/inline/text/verbatim      regex "(^|\h)([=]\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[=])\W"        2:meta
add-highlighter shared/org/inline/text/code          regex "(^|\h)([~]\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[~])\W"        2:mono
add-highlighter shared/org/inline/text/inline-math   regex "(^|\h)([$]\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[$])\W"        2:mono
add-highlighter shared/org/inline/text/underlined    regex "(^|\h)([_]\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[_])\W"        2:underline
add-highlighter shared/org/inline/text/bold          regex "(^[*][^*]|\h[*])(\S[\w\S\h]*?([\n]{1})?[\w\S\h]*?\S[*])\W" 0:bold
add-highlighter shared/org/inline/text/date          regex "(?:^|\h)([\[][^\s][^\n]*?[^\s]*?[\]])\W"                   0:variable
add-highlighter shared/org/inline/text/link          regex "(?:^|\h)([\[]{2}[^\n]*?[\]]{2})\W"                         0:link
add-highlighter shared/org/inline/text/drawer        regex "^\h*([:][^\s][^\n]*?[^\s]*?[:])\W"                         1:keyword

add-highlighter shared/org/math region '[$]{2}|\\\[' '[$]{2}|\\\]' fill mono
# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden org-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # follow list item indentation and amount of leading whitespaces
        try %{ execute-keys -draft 'k<a-x>s\S<ret><space><a-?>^\h*([-+]|\h+[*]|\d[.)])<ret><a-x>s^\h*([+-]|\h+[*][^*]|\d[.)])\h*<ret>y/^\n<ret>PxHr<space>' }
        try %{ execute-keys -draft -itersel 'k<a-x>s\h+$<ret>d' }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group org-highlight global WinSetOption filetype=org %{
    add-highlighter window/org ref org
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/org }
}

hook global WinSetOption filetype=org %{
    hook window InsertChar \n -group org-indent org-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window org-.+ }
}
