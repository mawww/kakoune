hook global BufCreate .*\.(z|ba|c|k)?sh(rc|_profile)? %{
    set buffer filetype sh
}

hook global BufSetOption mimetype=text/x-shellscript %{
    set buffer filetype sh
}

addhl -group / regions -default code sh \
    double_string  %{(?<!\\)(\\\\)*\K"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(?<!\\)(\\\\)*\K'} %{'} '' \
    comment '(?<!\$)#' '$' ''

addhl -group /sh/double_string fill string
addhl -group /sh/single_string fill string
addhl -group /sh/comment fill comment

addhl -group /sh/code regex \<(alias|bind|builtin|caller|case|cd|command|coproc|declare|do|done|echo|elif|else|enable|esac|exit|fi|for|function|help|if|in|let|local|logout|mapfile|printf|read|readarray|readonly|return|select|set|shift|source|test|then|time|type|typeset|ulimit|unalias|until|while)\> 0:keyword
addhl -group /sh/code regex [\[\]\(\)&|]{2}|\[\s|\s\] 0:operator
addhl -group /sh/code regex (\w+)= 1:identifier
addhl -group /sh/code regex ^\h*(\w+)\h*\(\) 1:identifier

addhl -group /sh/code regex \$(\w+|\{.+?\}|#|@|\?|\$|!|-|\*) 0:identifier
addhl -group /sh/double_string regex \$(\w+|\{.+?\}) 0:identifier

hook global WinSetOption filetype=sh %{ addhl ref sh }
hook global WinSetOption filetype=(?!sh).* %{ rmhl sh }
