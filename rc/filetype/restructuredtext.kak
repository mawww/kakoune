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

hook -group restructuredtext-load-languages global WinSetOption filetype=restructuredtext %{
    restructuredtext-load-languages '%'
}

hook -group restructuredtext-load-languages global WinSetOption filetype=restructuredtext %{
    hook -group restructuredtext-load-languages window NormalIdle .* %{restructuredtext-load-languages gtGbGl}
    hook -group restructuredtext-load-languages window InsertIdle .* %{restructuredtext-load-languages gtGbGl}
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
add-highlighter shared/restructuredtext/code region ::\h*\n ^(?=\S)  fill meta

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

define-command restructuredtext-load-languages -params 1 %{
    evaluate-commands -draft %{ try %{
        execute-keys "%arg{1}s^\.\.\h*code-block::\h*\K\w+<ret>"
        evaluate-commands -itersel %{ try %{
            require-module %val{selection}
            add-highlighter "shared/restructuredtext/%val{selection}" region "\.\.\h*code-block::\h*%val{selection}\h*\n" '^(?=\S)' regions
            add-highlighter "shared/restructuredtext/%val{selection}/" default-region fill meta
            add-highlighter "shared/restructuredtext/%val{selection}/inner" region \A\.\.\h*code-block::[^\n]*\K '^(?=\S)' ref %val{selection}
        }}
    }}
}

}
