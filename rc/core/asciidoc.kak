# http://asciidoc.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .+\.(a(scii)?doc|asc) %{
    set buffer filetype asciidoc
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / group asciidoc

addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n~{2,}\h*\n\h*$ 0:header
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n\^{2,}\h*\n\h*$ 0:header
addhl -group /asciidoc regex ^\h+([-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:list 1:bullet
addhl -group /asciidoc regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:block
addhl -group /asciidoc regex (?<!\w)(?:\+[^\n]+?\+|`[^\n]+?`)(?!\w) 0:mono
addhl -group /asciidoc regex (?<!\w)_[^\n]+?_(?!\w) 0:italic
addhl -group /asciidoc regex (?<!\w)\*[^\n]+?\*(?!\w) 0:bold
addhl -group /asciidoc regex ^:[-\w]+: 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
#
hook -group asciidoc-highlight global WinSetOption filetype=asciidoc %{ addhl ref asciidoc }
hook -group asciidoc-highlight global WinSetOption filetype=(?!asciidoc).* %{ rmhl asciidoc }
