hook global BufCreate .*\.asciidoc %{ setb filetype asciidoc }

hook global WinSetOption filetype=asciidoc %{
    addhl group asciidoc-highlight
    addhl -group asciidoc-highlight regex ^[^\n]+\n={2,}\h*$ 0:blue
    addhl -group asciidoc-highlight regex ^[^\n]+\n-{2,}\h*$ 0:cyan
    addhl -group asciidoc-highlight regex ^\h+[-\*]\h+([^\n:]+:)?[^\n]*(\n\h+[^-\*]\S+[^\n]*)*$ 0:yellow 1:green
}

hook global WinSetOption filetype=(?!asciidoc).* %{ rmhl asciidoc-higlight }

