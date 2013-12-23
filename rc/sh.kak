hook global BufCreate .*\.(sh) %{
    set buffer filetype sh
}

hook global BufSetOption mimetype=text/x-shellscript %{
    set buffer filetype sh
}

defhl sh
addhl -def-group sh regex \<(if|then|fi|while|for|do|done|case|esac|echo|cd|shift|return|exit|local)\> 0:keyword
addhl -def-group sh regex [\[\]\(\)&|]{2} 0:operator
addhl -def-group sh regex (\w+)= 1:identifier
addhl -def-group sh regex ^\h*(\w+)\h*\(\) 1:identifier
addhl -def-group sh regex "(^|\h)#.*?$" 0:comment
addhl -def-group sh regex (["'])(?:\\\1|.)*?\1 0:string
addhl -def-group sh regex \$(\w+|\{.+?\}) 0:identifier

hook global WinSetOption filetype=sh %{ addhl ref sh }
hook global WinSetOption filetype=(?!sh).* %{ rmhl sh }
