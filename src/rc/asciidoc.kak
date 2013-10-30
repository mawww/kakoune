hook global BufCreate .*\.asciidoc %{ set buffer filetype asciidoc }

hook global WinSetOption filetype=asciidoc %{
    addhl group asciidoc-highlight
    addhl -group asciidoc-highlight regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:blue
    addhl -group asciidoc-highlight regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:cyan
    addhl -group asciidoc-highlight regex ^\h+([-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:yellow 1:cyan
    addhl -group asciidoc-highlight regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:magenta
    addhl -group asciidoc-highlight regex (?<!\w)\+[^\n]+?\+(?!\w) 0:green
    addhl -group asciidoc-highlight regex (?<!\w)_[^\n]+?_(?!\w) 0:yellow
    addhl -group asciidoc-highlight regex (?<!\w)\*[^\n]+?\*(?!\w) 0:red
}

hook global WinSetOption filetype=(?!asciidoc).* %{ rmhl asciidoc-higlight }

