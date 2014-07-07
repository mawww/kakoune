# http://fishshell.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-fish %{
    set buffer filetype fish
}

hook global BufCreate .*[.](fish) %{
    set buffer filetype fish
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / multi_region -default code fish \
    double_string '"' (?<!\\)(\\\\)*"       '' \
    single_string "'" "'"                   '' \
    comment       '#' $                     ''

addhl -group /fish/double_string fill string
addhl -group /fish/double_string regex (\$\w+)|(\{\$\w+\}) 0:identifier
addhl -group /fish/single_string fill string
addhl -group /fish/comment       fill comment

addhl -group /fish/code regex (\$\w+)|(\{\$\w+\}) 0:identifier

# Command names are collected using `builtin --names` and 'eval' from `functions --names`
addhl -group /fish/code regex \<(and|begin|bg|bind|block|break|breakpoint|builtin|case|cd|command|commandline|complete|contains|continue|count|echo|else|emit|end|eval|exec|exit|fg|for|function|functions|history|if|jobs|not|or|printf|pwd|random|read|return|set|set_color|source|status|switch|test|ulimit|while)\> 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _fish_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h+$ <ret> d }
    }
}

def -hidden _fish_indent_on_char %{
    eval -draft -itersel %{
        # deindent on (else|end) command insertion
        try %{ exec -draft <space> <a-i>w <a-k> (else|end) <ret> <a-lt> }
    }
}

def -hidden _fish_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _fish_filter_around_selections <ret> }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
        # indent after (case|else) commands
        try %{ exec -draft <space> k x <a-k> (case|else) <ret> j <a-gt> }
        # indent after (begin|for|function|if|switch|while) commands and add 'end' command
        try %{ exec -draft <space> k x <a-k> (begin|for|function|(?<!(else)\h+)if|switch|while) <ret> x y p j a end <esc> k <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=fish %{
    addhl ref fish

    hook window InsertEnd  .* -group fish-hooks  _fish_filter_around_selections
    hook window InsertChar .* -group fish-indent _fish_indent_on_char
    hook window InsertChar \n -group fish-indent _fish_indent_on_new_line
}

hook global WinSetOption filetype=(?!fish).* %{
    rmhl fish
    rmhooks window fish-indent
    rmhooks window fish-hooks
}
