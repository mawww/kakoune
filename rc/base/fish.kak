# http://fishshell.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](fish) %{
    set buffer filetype fish
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code fish \
    double_string '"' (?<!\\)(\\\\)*"  '' \
    single_string "'" "'"              '' \
    comment       '#' '$'              ''

add-highlighter -group /fish/double_string fill string
add-highlighter -group /fish/double_string regex (\$\w+)|(\{\$\w+\}) 0:variable
add-highlighter -group /fish/single_string fill string
add-highlighter -group /fish/comment       fill comment

add-highlighter -group /fish/code regex (\$\w+)|(\{\$\w+\}) 0:variable

# Command names are collected using `builtin --names` and 'eval' from `functions --names`
add-highlighter -group /fish/code regex \b(and|begin|bg|bind|block|break|breakpoint|builtin|case|cd|command|commandline|complete|contains|continue|count|echo|else|emit|end|eval|exec|exit|fg|for|function|functions|history|if|jobs|not|or|printf|pwd|random|read|return|set|set_color|source|status|switch|test|ulimit|while)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden fish-filter-around-selections %{
    eval -no-hooks -draft -itersel %{
        # remove trailing white spaces
        try %{ exec -draft <a-x>s\h+$<ret>d }
    }
}

def -hidden fish-indent-on-char %{
    eval -no-hooks -draft -itersel %{
        # align middle and end structures to start and indent when necessary
        try %{ exec -draft <a-x><a-k>^\h*(else)$<ret><a-\;><a-?>^\h*(if)<ret>s\A|\Z<ret>'<a-&> }
        try %{ exec -draft <a-x><a-k>^\h*(end)$<ret><a-\;><a-?>^\h*(begin|for|function|if|switch|while)<ret>s\A|\Z<ret>'<a-&> }
        try %{ exec -draft <a-x><a-k>^\h*(case)$<ret><a-\;><a-?>^\h*(switch)<ret>s\A|\Z<ret>'<a-&>'<space><a-gt> }
    }
}

def -hidden fish-indent-on-new-line %{
    eval -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space>K<a-&> }
        # filter previous line
        try %{ exec -draft k:fish-filter-around-selections<ret> }
        # indent after start structure
        try %{ exec -draft k<a-x><a-k>^\h*(begin|case|else|for|function|if|switch|while)\b<ret>j<a-gt> }
    }
}

def -hidden fish-insert-on-new-line %{
    eval -no-hooks -draft -itersel %{
        # copy _#_ comment prefix and following white spaces
        try %{ exec -draft k<a-x>s^\h*\K#\h*<ret>yjp }
        # wisely add end structure
        eval -save-regs x %{
            try %{ exec -draft k<a-x>s^\h+<ret>"xy } catch %{ reg x '' }
            try %{ exec -draft k<a-x><a-k>^<c-r>x(begin|for|function|if|switch|while)<ret>j<a-a>iX<a-\;>K<a-K>^<c-r>x(begin|for|function|if|switch|while).*\n<c-r>xend$<ret>jxypjaend<esc><a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group fish-highlight global WinSetOption filetype=fish %{ add-highlighter ref fish }

hook global WinSetOption filetype=fish %{
    hook window InsertChar .* -group fish-indent fish-indent-on-char
    hook window InsertChar \n -group fish-insert fish-insert-on-new-line
    hook window InsertChar \n -group fish-indent fish-indent-on-new-line
}

hook -group fish-highlight global WinSetOption filetype=(?!fish).* %{ remove-highlighter fish }

hook global WinSetOption filetype=(?!fish).* %{
    remove-hooks window fish-indent
    remove-hooks window fish-insert
}
