# https://www.latex-project.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(tex|cls|sty|dtx) %{
    set-option buffer filetype latex
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=latex %(
    require-module latex

    hook window InsertChar \n -group latex-indent %{ latex-indent-newline }
    hook window InsertChar \} -group latex-indent %{ latex-indent-closing-brace }
    hook window ModeChange pop:insert:.* -group latex-indent %{ latex-trim-indent }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window latex-indent }
    hook window InsertChar \n -group latex-insert latex-insert-on-new-line
)

hook -group latex-highlight global WinSetOption filetype=latex %{
    add-highlighter window/latex ref latex
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/latex }
}

provide-module latex %~

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/latex regions
add-highlighter shared/latex/content default-region group
# Region for control sequence (includes latex2e arguments and options)
# starting with unescaped \ and ending :
# - at eol, or
# - at word boundaries not preceded nor followed by @ : \ { } [ ] *, or
# - after an unescaped }
add-highlighter shared/latex/cs region '(?<!\\)(?:\\\\)*\K\\[@\w]' '/\n|(?<![@:\\{}\[\]*])(?![@:\\{}\[\]*])\b|(?<!\\)(?:\\\\)*\K\}\K' group
add-highlighter shared/latex/comment region '(?<!\\)(?:\\\\)*\K%' '\n' fill comment

# Document and LaTeX2e control sequence
add-highlighter shared/latex/cs/ regex '(?:\\[a-zA-Z@]+)' 0:keyword
## Options passed to LaTeX2e control sequences, between brackets
add-highlighter shared/latex/cs/ regex '\\[a-zA-Z@]+\b\[([^\]]+)\]' 1:value
## Emphasized text
add-highlighter shared/latex/cs/ regex '\\(?:emph|textit|textsl)\{([^}]+)\}' 1:default+i
## Underlined text
add-highlighter shared/latex/cs/ regex '\\underline\{([^}]+)\}' 1:default+u
## Bold text
add-highlighter shared/latex/cs/ regex '\\textbf\{([^}]+)\}' 1:default+b
## Section headings
add-highlighter shared/latex/cs/ regex '\\(part|section)\*?\{([^}]+)\}' 2:title
add-highlighter shared/latex/cs/ regex '\\(chapter|(sub)+section|(sub)*paragraph)\*?\{([^}]+)\}' 4:header

# LaTeX3 control sequence
## Functions (expl3 doc) module_name:arguments_types.
add-highlighter shared/latex/cs/ regex '\\(?:__|@@_)?[a-zA-Z@]+_\w+(:[nNpTFDwcVvxefo]+)?' 0:function 1:+db@type
## Variables (expl3 doc): scope_name_type
add-highlighter shared/latex/cs/ regex '\\([lgc]_)[a-zA-Z@]+_\w+' 0:variable 1:+db
## l3kernel modules (l3kernel/doc/l3prefixes.csv)
add-highlighter shared/latex/cs/ regex '\\(alignment|alloc|ampersand|atsign|backslash|bitset|bool|box|catcode|cctab|char|chk|circumflex|clist|code|codedoc|coffin|colon|color|cs|debug|dim|document|dollar|driver|e|else|empty|etex|exp|expl|false|fi|file|flag|fp|group|hash|hbox|hcoffin|if|inf|initex|insert|int|intarray|ior|iow|job|kernel|keys|keyval|left|log|lua|luatex|mark|marks|math|max|minus|mode|msg|muskip|nan|nil|no|novalue|one|or|other|parameter|pdf|pdftex|peek|percent|pi|prg|prop|ptex|quark|recursion|ref|regex|reverse|right|scan|seq|skip|sort|space|stop|str|sys|tag|term|tex|text|tilde|tl|tmpa|tmpb|token|true|underscore|uptex|use|utex|vbox|vcoffin|xetex|zero)_' 0:+db
# LaTeX3 types (expl3 doc)
add-highlighter shared/latex/cs/ regex '_(bool|box|cctab|clist|coffin|dim|fp|ior|iow|int|muskip|prop|seq|skip|str|tl)\b' 0:+db

# This belongs to content group as the LaTeX3 convention is separating macros names, args and options
# with spaces and thus should not be catched by the cs region
## macros arguments
add-highlighter shared/latex/content/ regex '(?<!\\)(?:\\\\)*\K#+[1-9]' 0:string
## group containing words and numbers (list separated by ; , / or spaces)
add-highlighter shared/latex/content/ regex '(?<!\\)(?:\\\\)*\K\{([\s/;,.\w\d]+)\}' 1:string

# Math mode between dollar signs/pairs
add-highlighter shared/latex/content/ regex '((?<!\\)(?:\\\\)*\K\$(\\\$|[^$])+\$)|((?<!\\)(?:\\\\)*\K\$\$(\\\$|[^$])+\$\$)|((?<!\\)(?:\\\\)*\K\\\[.*?\\\])|(\\\(.*?\\\))' 0:meta

# Indent
# ------

define-command -hidden latex-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        try %{ execute-keys x 1s^(\h+)$<ret> d }
    }
}

define-command -hidden latex-indent-newline %(
    evaluate-commands -no-hooks -draft -itersel %(
        # copy '%' comment prefix and following white spaces
        try %{ execute-keys -draft kx s^\h*%\h*<ret> y jgh P }
        # preserve previous line indent
        try %{ execute-keys -draft K<a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft kx s\h+$<ret> d }
        # indent after line ending with {
        try %( execute-keys -draft kx <a-k>\{$<ret> j<a-gt> )
        # deindent closing brace(s) when after cursor
        try %( execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> )
        # indent after line ending with \begin{...}[...]{...}, with multiple
        # sets of arguments possible
        try %(
            execute-keys -draft \
                kx \
                <a-k>\\begin\h*\{[^\}]+\}(\h|\[.*\]|\{.*\})*$<ret> \
                j<a-gt>
        )
    )
)

define-command -hidden latex-indent-closing-brace %(
    evaluate-commands -no-hooks -draft -itersel %(
        # Align lone } with matching bracket
        try %( execute-keys -draft x_ <a-k>\A\}\z<ret> m<a-S>1<a-&> )
        # Align \end{...} with corresponding \begin{...}
        try %(
            execute-keys -draft h<a-h> 1s\\end\h*\{([^\}]+)\}\z<ret> \
                <a-?>\\begin\s*\{<c-r>.\}<ret> <a-S>1<a-&>
        )
    )
)

define-command -hidden latex-insert-on-new-line %(
    evaluate-commands -no-hooks -draft -itersel %(
        # Wisely add "\end{...}".
        evaluate-commands -save-regs xz %(
            # Save previous line indent in register x.
            try %( execute-keys -draft kxs^\h+<ret>"xy ) catch %( reg x '' )
            # Save item of begin in register z.
            try %( execute-keys -draft kxs\{.*\}<ret>"zy ) catch %( reg z '' )
            try %(
                # Validate previous line and that it is not closed yet.
                execute-keys -draft kx <a-k>^<c-r>x\h*\\begin\{.*\}<ret> J}iJx <a-K>^<c-r>x(\\end\<c-r>z<backspace>\})<ret>
                # Auto insert "\end{...}".
                execute-keys -draft o<c-r>x\end<c-r>z<esc>
            )
        )
    )
)

~
