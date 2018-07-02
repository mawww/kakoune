# http://fishshell.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](fish) %{
    set-option buffer filetype fish
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/fish regions
add-highlighter shared/fish/code default-region group
add-highlighter shared/fish/double_string region '"' (?<!\\)(\\\\)*"  group
add-highlighter shared/fish/single_string region "'" "'"              fill string
add-highlighter shared/fish/comment       region '#' '$'              fill comment

add-highlighter shared/fish/double_string/ fill string
add-highlighter shared/fish/double_string/ regex (\$\w+)|(\{\$\w+\}) 0:variable

add-highlighter shared/fish/code/ regex (\$\w+)|(\{\$\w+\}) 0:variable

# Command names are collected using `builtin --names` and 'eval' from `functions --names`
add-highlighter shared/fish/code/ regex \b(and|begin|bg|bind|block|break|breakpoint|builtin|case|cd|command|commandline|complete|contains|continue|count|echo|else|emit|end|eval|exec|exit|fg|for|function|functions|history|if|jobs|not|or|printf|pwd|random|read|return|set|set_color|source|status|switch|test|ulimit|while)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden fish-filter-around-selections %{
    evaluate-commands -no-hooks -draft -itersel %{
        # remove trailing white spaces
        try %{ execute-keys -draft <a-x>s\h+$<ret>d }
    }
}

define-command -hidden fish-indent-on-char %{
    evaluate-commands -no-hooks -draft -itersel %{
        # align middle and end structures to start and indent when necessary
        try %{ execute-keys -draft <a-x><a-k>^\h*(else)$<ret><a-\;><a-?>^\h*(if)<ret>s\A|\z<ret>)<a-&> }
        try %{ execute-keys -draft <a-x><a-k>^\h*(end)$<ret><a-\;><a-?>^\h*(begin|for|function|if|switch|while)<ret>s\A|\z<ret>)<a-&> }
        try %{ execute-keys -draft <a-x><a-k>^\h*(case)$<ret><a-\;><a-?>^\h*(switch)<ret>s\A|\z<ret>)<a-&>)<space><a-gt> }
    }
}

define-command -hidden fish-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <space>K<a-&> }
        # filter previous line
        try %{ execute-keys -draft k:fish-filter-around-selections<ret> }
        # indent after start structure
        try %{ execute-keys -draft k<a-x><a-k>^\h*(begin|case|else|for|function|if|switch|while)\b<ret>j<a-gt> }
    }
}

define-command -hidden fish-insert-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # copy _#_ comment prefix and following white spaces
        try %{ execute-keys -draft k<a-x>s^\h*\K#\h*<ret>yjp }
        # wisely add end structure
        evaluate-commands -save-regs x %{
            try %{ execute-keys -draft k<a-x>s^\h+<ret>"xy } catch %{ reg x '' }
            try %{ execute-keys -draft k<a-x><a-k>^<c-r>x(begin|for|function|if|switch|while)<ret>j<a-a>iX<a-\;>K<a-K>^<c-r>x(begin|for|function|if|switch|while).*\n<c-r>xend$<ret>jxypjaend<esc><a-lt> }
        }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group fish-highlight global WinSetOption filetype=fish %{ add-highlighter window/fish ref fish }

hook global WinSetOption filetype=fish %{
    hook window InsertChar .* -group fish-indent fish-indent-on-char
    hook window InsertChar \n -group fish-insert fish-insert-on-new-line
    hook window InsertChar \n -group fish-indent fish-indent-on-new-line
}

hook -group fish-highlight global WinSetOption filetype=(?!fish).* %{ remove-highlighter window/fish }

hook global WinSetOption filetype=(?!fish).* %{
    remove-hooks window fish-indent
    remove-hooks window fish-insert
}
