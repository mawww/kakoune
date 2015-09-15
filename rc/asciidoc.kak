# http://asciidoc.org/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.asciidoc %{ set buffer filetype asciidoc }

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / group asciidoc

addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n~{2,}\h*\n\h*$ 0:header
addhl -group /asciidoc regex (\A|\n\n)[^\n]+\n\^{2,}\h*\n\h*$ 0:header
addhl -group /asciidoc regex ^\h+([-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:list 1:bullet
addhl -group /asciidoc regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:block
addhl -group /asciidoc regex \B\+[^\n]+?\+\B 0:mono
addhl -group /asciidoc regex \b_[^\n]+?_\b 0:italic
addhl -group /asciidoc regex \B\*[^\n]+?\*\B 0:bold
addhl -group /asciidoc regex ^:[-\w]+: 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾
#
hook global WinSetOption filetype=asciidoc %{ addhl ref asciidoc }
hook global WinSetOption filetype=(?!asciidoc).* %{ rmhl asciidoc }
