hook global BufCreate .*\.asciidoc %{ set buffer filetype asciidoc }

addhl -group / group asciidoc
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:blue
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:cyan
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n~{2,}\h*\n\h*$ 0:green
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n\^{2,}\h*\n\h*$ 0:yellow
addhl -group /asciidoc regex ^\h+([-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:yellow 1:cyan
addhl -group /asciidoc regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:magenta
addhl -group /asciidoc regex (?<!\w)\+[^\n]+?\+(?!\w) 0:green
addhl -group /asciidoc regex (?<!\w)_[^\n]+?_(?!\w) 0:yellow
addhl -group /asciidoc regex (?<!\w)\*[^\n]+?\*(?!\w) 0:red
addhl -group /asciidoc regex ^:[-\w]+: 0:blue

hook global WinSetOption filetype=asciidoc %{ addhl ref asciidoc }
hook global WinSetOption filetype=(?!asciidoc).* %{ rmhl asciidoc }

