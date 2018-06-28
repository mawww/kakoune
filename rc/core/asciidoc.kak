# http://asciidoc.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .+\.(a(scii)?doc|asc) %{
    set-option buffer filetype asciidoc
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/asciidoc group

add-highlighter shared/asciidoc/ regex (\A|\n\n)[^\n]+\n={2,}\h*$ 0:title
add-highlighter shared/asciidoc/ regex (\A|\n\n)[^\n]+\n-{2,}\h*$ 0:header
add-highlighter shared/asciidoc/ regex (\A|\n\n)[^\n]+\n~{2,}\h*$ 0:header
add-highlighter shared/asciidoc/ regex (\A|\n\n)[^\n]+\n\^{2,}\h*$ 0:header

add-highlighter shared/asciidoc/ regex (\A|\n\n)=\h+[^\n]+$ 0:title
add-highlighter shared/asciidoc/ regex (\A|\n\n)={2,}\h+[^\n]+$ 0:header

add-highlighter shared/asciidoc/ regex ^\h+([-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:list 1:bullet
add-highlighter shared/asciidoc/ regex ^(-{3,})\n[^\n\h].*?\n(-{3,})$ 0:block
add-highlighter shared/asciidoc/ regex ^(={3,})\n[^\n\h].*?\n(={3,})$ 0:block
add-highlighter shared/asciidoc/ regex ^(~{3,})\n[^\n\h].*?\n(~{3,})$ 0:block
add-highlighter shared/asciidoc/ regex ^(\*{3,})\n[^\n\h].*?\n(\*{3,})$ 0:block
add-highlighter shared/asciidoc/ regex \B(?:\+[^\n]+?\+|`[^\n]+?`)\B 0:mono
add-highlighter shared/asciidoc/ regex \b_[^\n]+?_\b 0:italic
add-highlighter shared/asciidoc/ regex \B\*[^\n]+?\*\B 0:bold
add-highlighter shared/asciidoc/ regex ^:[-\w]+: 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
#
hook -group asciidoc-highlight global WinSetOption filetype=asciidoc %{ add-highlighter window/asciidoc ref asciidoc }
hook -group asciidoc-highlight global WinSetOption filetype=(?!asciidoc).* %{ remove-highlighter window/asciidoc }
