# http://daringfireball.net/projects/markdown
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-markdown %{
    set buffer filetype markdown
}

hook global BufCreate .*[.](markdown|md|mkd) %{
    set buffer filetype markdown
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default content markdown \
    sh         ```sh   ```                    '' \
    fish       ```fish ```                    '' \
    ruby       ```ruby ```                    '' \
    code       ```     ```                    '' \
    code       ``       ``                    '' \
    code       `         `                    ''

addhl -group /markdown/code fill meta

addhl -group /markdown/sh   ref sh
addhl -group /markdown/fish ref fish
addhl -group /markdown/ruby ref ruby

# Setext-style header
addhl -group /markdown/content regex (\A|\n\n)[^\n]+\n={2,}\h*\n\h*$ 0:title
addhl -group /markdown/content regex (\A|\n\n)[^\n]+\n-{2,}\h*\n\h*$ 0:header

# Atx-style header
addhl -group /markdown/content regex ^(#+)(\h+)([^\n]+) 1:header

addhl -group /markdown/content regex ^\h?+((?:[\s\t]+)?[-\*])\h+[^\n]*(\n\h+[^-\*]\S+[^\n]*\n)*$ 0:list 1:bullet
addhl -group /markdown/content regex ^([-=~]+)\n[^\n\h].*?\n\1$ 0:block
addhl -group /markdown/content regex \B\+[^\n]+?\+\B 0:mono
addhl -group /markdown/content regex \b_[^\n]+?_\b 0:italic
addhl -group /markdown/content regex \B\*[^\n]+?\*\B 0:bold
addhl -group /markdown/content regex <[a-z]+://.*?> 0:link
addhl -group /markdown/content regex ^\h*(>\h*)+ 0:comment
addhl -group /markdown/content regex \H\K\h\h$ 0:PrimarySelection

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _markdown_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _markdown_filter_around_selections <ret> }
        # copy block quote(s), list item prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K((>\h*)|[*+-])+\h* <ret> y j p }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=markdown %{
    addhl ref markdown
    hook window InsertChar \n -group markdown-indent _markdown_indent_on_new_line
}

hook global WinSetOption filetype=(?!markdown).* %{
    rmhl markdown
    rmhooks window markdown-indent
}
