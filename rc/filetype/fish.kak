# http://fishshell.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](fish) %{
    set-option buffer filetype fish
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=fish %{
    require-module fish

    hook window InsertChar .* -group fish-indent fish-indent-on-char
    hook window InsertChar \n -group fish-indent fish-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window fish-.+ }
}

hook -group fish-highlight global WinSetOption filetype=fish %{
    add-highlighter window/fish ref fish
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/fish }
}


provide-module fish %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/fish regions
add-highlighter shared/fish/code default-region group
add-highlighter shared/fish/double_string region (?<!\\)(?:\\\\)*\K" (?<!\\)(\\\\)*"  group
add-highlighter shared/fish/single_string region (?<!\\)(?:\\\\)*\K' (?<!\\)(\\\\)*'  fill string
add-highlighter shared/fish/comment       region (?<!\\)(?:\\\\)*(?:^|\h)\K# '$' fill comment

add-highlighter shared/fish/double_string/ fill string
add-highlighter shared/fish/double_string/ regex (\$\w+)|(\{\$\w+\}) 0:variable

add-highlighter shared/fish/code/ regex (\$\w+)|(\{\$\w+\}) 0:variable

# Command names are collected using `builtin --names`.
add-highlighter shared/fish/code/ regex \b(and|argparse|begin|bg|bind|block|break|breakpoint|builtin|case|cd|command|commandline|complete|contains|continue|count|disown|echo|else|emit|end|eval|exec|exit|false|fg|for|function|functions|history|if|jobs|math|not|or|printf|pwd|random|read|realpath|return|set|set_color|source|status|switch|test|time|ulimit|wait|while)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden fish-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        # remove trailing white spaces
        try %{ execute-keys -draft <a-x> 1s^(\h+)$<ret> d }
    }
}

define-command -hidden fish-indent-on-char %{
    evaluate-commands -no-hooks -draft -itersel %{
        # align middle and end structures to start and indent when necessary
        try %{ execute-keys -draft <a-x><a-k>^\h*(else)$<ret><a-semicolon><a-?>^\h*(if)<ret>s\A|.\z<ret>1<a-&> }
        try %{ execute-keys -draft <a-x><a-k>^\h*(end)$<ret><a-semicolon><a-?>^\h*(begin|for|function|if|switch|while)<ret>s\A|.\z<ret>1<a-&> }
        try %{ execute-keys -draft <a-x><a-k>^\h*(case)$<ret><a-semicolon><a-?>^\h*(switch)<ret>s\A|.\z<ret>1<a-&> }
    }
}

define-command -hidden fish-indent-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*#\h* <ret> y jgh P }
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # cleanup trailing whitespaces from previous line
        try %{ execute-keys -draft k <a-x> s \h+$ <ret> d }
        # indent after start structure
        try %{ execute-keys -draft k<a-x><a-k>^\h*(begin|case|else|for|function|if|while)\b<ret>j<a-gt> }
    }
}

}
