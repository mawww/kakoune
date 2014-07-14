hook global BufCreate .*\.(sh) %{
    set buffer filetype sh
}

hook global BufSetOption mimetype=text/x-shellscript %{
    set buffer filetype sh
}

addhl -group / multi_region -default code sh \
    double_string  %{(^|\h)"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(^|\h)'} %{(?<!\\)(\\\\)*'} '' \
    comment '#' '$' ''

addhl -group /sh/double_string fill string
addhl -group /sh/single_string fill string
addhl -group /sh/comment fill comment

addhl -group /sh/code regex \<(if|then|fi|while|for|do|done|case|esac|echo|cd|shift|return|exit|local)\> 0:keyword
addhl -group /sh/code regex [\[\]\(\)&|]{2} 0:operator
addhl -group /sh/code regex (\w+)= 1:identifier
addhl -group /sh/code regex ^\h*(\w+)\h*\(\) 1:identifier

addhl -group /sh/code regex \$(\w+|\{.+?\}) 0:identifier
addhl -group /sh/double_string regex \$(\w+|\{.+?\}) 0:identifier

hook global WinSetOption filetype=sh %{ addhl ref sh }
hook global WinSetOption filetype=(?!sh).* %{ rmhl sh }
