# http://scala-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scala) %{
    set buffer filetype scala
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code scala \
    string  '"' (?<!\\)(\\\\)*"         '' \
    literal  `    `                     '' \
    comment  //   $                     '' \
    comment /[*] [*]/                 /[*]

addhl -group /scala/string  fill string
addhl -group /scala/literal fill identifier
addhl -group /scala/comment fill comment

# Keywords are collected at
# http://tutorialspoint.com/scala/scala_basic_syntax.htm

addhl -group /scala/code regex \b(import|package)\b 0:meta
addhl -group /scala/code regex \b(this|true|false|null)\b 0:value
addhl -group /scala/code regex \b(become|case|catch|class|def|do|else|extends|final|finally|for|forSome|goto|if|initialize|macro|match|new|object|onTransition|return|startWith|stay|throw|trait|try|unbecome|using|val|var|when|while|with|yield)\b 0:keyword
addhl -group /scala/code regex \b(abstract|final|implicit|implicitly|lazy|override|private|protected|require|sealed|super)\b 0:attribute
addhl -group /scala/code regex \b(⇒|=>|<:|:>|=:=|::|&&|\|\|)\b 0:operator
addhl -group /scala/code regex "'[_A-Za-z0-9$]+" 0:identifier

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _scala_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _scala_indent_on_new_line %[
    eval -draft -itersel %[
        # preserve previous line indent
        try %[ exec -draft <space> K <a-&> ]
        # filter previous line
        try %[ exec -draft k : _scala_filter_around_selections <ret> ]
        # copy // comments prefix and following white spaces
        try %[ exec -draft k x s ^\h*\K#\h* <ret> y j p ]
        # indent after lines ending with {
        try %[ exec -draft k x <a-k> \{$ <ret> j <a-gt> ]
    ]
]

def -hidden _scala_indent_on_closing_curly_brace %[
    eval -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ exec -draft <a-h> <a-k> ^\h+\}$ <ret> m s \`|.\' <ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scala-highlight global WinSetOption filetype=scala %{ addhl ref scala }

hook global WinSetOption filetype=scala %[
    hook window InsertEnd  .* -group scala-hooks  _scala_filter_around_selections
    hook window InsertChar \n -group scala-indent _scala_indent_on_new_line
    hook window InsertChar \} -group scala-indent _scala_indent_on_closing_curly_brace
]

hook -group scala-highlight global WinSetOption filetype=(?!scala).* %{ rmhl scala }

hook global WinSetOption filetype=(?!scala).* %{
    rmhooks window scala-indent
    rmhooks window scala-hooks
}
