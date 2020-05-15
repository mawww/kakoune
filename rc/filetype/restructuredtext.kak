# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](rst) %{
    set-option buffer filetype restructuredtext
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=restructuredtext %{
    require-module restructuredtext
}

hook -group restructuredtext-highlight global WinSetOption filetype=restructuredtext %{
    add-highlighter window/restructuredtext ref restructuredtext
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/restructuredtext }
}

provide-module restructuredtext %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/restructuredtext regions
add-highlighter shared/restructuredtext/content default-region group
add-highlighter shared/restructuredtext/code region ::\h*\n ^[^\s]  fill meta

evaluate-commands %sh{
    for ft in c cabal clojure coffee cpp css cucumber ddiff dockerfile \
              fish gas go haml haskell html ini java javascript json \
              julia kak kickstart latex lisp lua makefile moon objc \
              perl pug python ragel ruby rust sass scala scss sh swift \
              tupfile yaml; do
        if [ "$ft" = kak ]; then ref="kakrc"; else ref="$ft"; fi
        printf 'add-highlighter shared/restructuredtext/%s region %s %s ref %s\n' "$ft" '\.\.\h*'$ft'::\h*c\h*\n' '^\S' "$ref"
    done
}

# Setext-style header
# Valid header characters:
# # ! " $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ ` { | } ~

add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(#{3,}\n)?[^\n]+\n(#{3,})$ 0:title
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(!{3,}\n)?[^\n]+\n(!{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)("{3,}\n)?[^\n]+\n("{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\${3,}\n)?[^\n]+\n(\${3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(%{3,}\n)?[^\n]+\n(%{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(&{3,}\n)?[^\n]+\n(&{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)('{3,}\n)?[^\n]+\n('{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\({3,}\n)?[^\n]+\n(\({3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\){3,}\n)?[^\n]+\n(\){3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\*{3,}\n)?[^\n]+\n(\*{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\+{3,}\n)?[^\n]+\n(\+{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(,{3,}\n)?[^\n]+\n(,{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(-{3,}\n)?[^\n]+\n(-{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\.{3,}\n)?[^\n]+\n(\.{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(/{3,}\n)?[^\n]+\n(/{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(:{3,}\n)?[^\n]+\n(:{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\;{3,}\n)?[^\n]+\n(\;{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(<{3,}\n)?[^\n]+\n(<{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(={3,}\n)?[^\n]+\n(={3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(>{3,}\n)?[^\n]+\n(>{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\?{3,}\n)?[^\n]+\n(\?{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(@{3,}\n)?[^\n]+\n(@{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\[{3,}\n)?[^\n]+\n(\[{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\\{3,}\n)?[^\n]+\n(\\{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\]{3,}\n)?[^\n]+\n(\]{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\^{3,}\n)?[^\n]+\n(\^{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(_{3,}\n)?[^\n]+\n(_{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(`{3,}\n)?[^\n]+\n(`{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\{{3,}\n)?[^\n]+\n(\{{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\|{3,}\n)?[^\n]+\n(\|{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(\}{3,}\n)?[^\n]+\n(\}{3,})$ 0:header
add-highlighter shared/restructuredtext/content/ regex (\A|\n\n)(~{3,}\n)?[^\n]+\n(~{3,})$ 0:header

# Inline markup
add-highlighter shared/restructuredtext/content/ regex [^*](\*\*([^\s*]|([^\s*][^*]*[^\s*]))\*\*)[^*] 1:+b
add-highlighter shared/restructuredtext/content/ regex [^*](\*([^\s*]|([^\s*][^*]*[^\s*]))\*)[^*] 1:+i
add-highlighter shared/restructuredtext/content/ regex [^`](``([^\s`]|([^\s`][^`]*[^\s`]))``)[^`] 1:mono

}
