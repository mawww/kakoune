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

addhl -group / regions -default code fish \
    double_string '"' (?<!\\)(\\\\)*"  '' \
    single_string "'" "'"              '' \
    comment       '#' '$'              ''

addhl -group /fish/double_string fill string
addhl -group /fish/double_string regex (\$\w+)|(\{\$\w+\}) 0:identifier
addhl -group /fish/single_string fill string
addhl -group /fish/comment       fill comment

addhl -group /fish/code regex (\$\w+)|(\{\$\w+\}) 0:identifier

# Command names are collected using `builtin --names` and 'eval' from `functions --names`
addhl -group /fish/code regex \b(and|begin|bg|bind|block|break|breakpoint|builtin|case|cd|command|commandline|complete|contains|continue|count|echo|else|emit|end|eval|exec|exit|fg|for|function|functions|history|if|jobs|not|or|printf|pwd|random|read|return|set|set_color|source|status|switch|test|ulimit|while)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _fish_filter_around_selections %{
    eval -no-hooks -draft -itersel %{
        # remove trailing white spaces
        try %{ exec -draft <a-x>s\h+$<ret>d }
    }
}

def -hidden _fish_indent_on_char %{
    eval -no-hooks -draft -itersel %{
        # align middle and end structures to start and indent when necessary
        try %{ exec -draft <a-x><a-k>^\h*(else)$<ret><a-\;><a-?>^\h*(if)<ret>s\A|\Z<ret>'<a-&> }
        try %{ exec -draft <a-x><a-k>^\h*(end)$<ret><a-\;><a-?>^\h*(begin|for|function|if|switch|while)<ret>s\A|\Z<ret>'<a-&> }
        try %{ exec -draft <a-x><a-k>^\h*(case)$<ret><a-\;><a-?>^\h*(switch)<ret>s\A|\Z<ret>'<a-&>'<space><a-gt> }
    }
}

def -hidden _fish_indent_on_new_line %{
    eval -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space>K<a-&> }
        # filter previous line
        try %{ exec -draft k:_fish_filter_around_selections<ret> }
        # indent after start structure
        try %{ exec -draft kx<a-k>^\h*(begin|case|else|for|function|if|switch|while)\b<ret>j<a-gt> }
    }
}

def -hidden _fish_insert_on_new_line %{
    eval -no-hooks -draft -itersel %{
        # copy _#_ comment prefix and following white spaces
        try %{ exec -draft kxs^\h*\K#\h*<ret>yjp }
        # wisely add end structure
        eval -save-regs x %{
            try %{ exec -draft kxs^\h+<ret>"xy } catch %{ reg x '' }
            try %{ exec -draft kx<a-k>^<c-r>x(begin|for|function|if|switch|while)<ret>j<a-a>iX<a-\;>K<a-K>^<c-r>x(begin|for|function|if|switch|while).*\n<c-r>xend$<ret>jxypjaend<esc><a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group fish-highlight global WinSetOption filetype=fish %{ addhl ref fish }

hook global WinSetOption filetype=fish %{
    hook window InsertChar .* -group fish-indent _fish_indent_on_char
    hook window InsertChar \n -group fish-indent _fish_indent_on_new_line
    hook window InsertChar \n -group fish-insert _fish_insert_on_new_line
}

hook -group fish-highlight global WinSetOption filetype=(?!fish).* %{ rmhl fish }

hook global WinSetOption filetype=(?!fish).* %{
    rmhooks window fish-indent
    rmhooks window fish-insert
}
