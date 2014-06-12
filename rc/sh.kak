hook global BufCreate .*\.(sh) %{
    set buffer filetype sh
}

hook global BufSetOption mimetype=text/x-shellscript %{
    set buffer filetype sh
}

addhl -group / group sh
addhl -group /sh regex \<(if|then|fi|while|for|do|done|case|esac|echo|cd|shift|return|exit|local)\> 0:keyword
addhl -group /sh regex [\[\]\(\)&|]{2} 0:operator
addhl -group /sh regex (\w+)= 1:identifier
addhl -group /sh regex ^\h*(\w+)\h*\(\) 1:identifier
addhl -group /sh regex "(^|\h)#.*?$" 0:comment
#addhl -group /sh regex (["'])(?:\\\1|.)*?\1 0:string

addhl -group /sh region double_string %{(^|\h)"} %{(?<!\\)(\\\\)*"}
addhl -group /sh/double_string/content fill string

addhl -group /sh region single_string %{(^|\h)'} %{(?<!\\)(\\\\)*'}
addhl -group /sh/single_string/content fill string

addhl -group /sh regex \$(\w+|\{.+?\}) 0:identifier

hook global WinSetOption filetype=sh %{ addhl ref sh }
hook global WinSetOption filetype=(?!sh).* %{ rmhl sh }
