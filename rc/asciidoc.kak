hook global BufCreate .*\.asciidoc %{ set buffer filetype asciidoc }

defhl asciidoc
addhl -def-group asciidoc regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:blue
addhl -def-group asciidoc regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:cyan
addhl -def-group asciidoc regex ^\h+([-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:yellow 1:cyan
addhl -def-group asciidoc regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:magenta
addhl -def-group asciidoc regex (?<!\w)\+[^\n]+?\+(?!\w) 0:green
addhl -def-group asciidoc regex (?<!\w)_[^\n]+?_(?!\w) 0:yellow
addhl -def-group asciidoc regex (?<!\w)\*[^\n]+?\*(?!\w) 0:red

hook global WinSetOption filetype=asciidoc %{ addhl ref asciidoc }
hook global WinSetOption filetype=(?!asciidoc).* %{ rmhl asciidoc }

