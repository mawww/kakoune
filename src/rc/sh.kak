hook global BufCreate .*\.(sh) %{
    set buffer filetype sh
}

hook global BufSetOption mimetype=text/x-shellscript %{
    set buffer filetype sh
}

hook global WinSetOption filetype=sh %~
    addhl group sh-highlight
    addhl -group sh-highlight regex \<(if|then|fi|while|for|do|done|case|esac|echo|cd|shift|return|exit|local)\> 0:keyword
    addhl -group sh-highlight regex [\[\]\(\)&|]{2} 0:operator
    addhl -group sh-highlight regex (\w+)= 1:identifier
    addhl -group sh-highlight regex ^\h*(\w+)\h*\(\) 1:identifier
    addhl -group sh-highlight regex "(^|\h)#.*?$" 0:comment
    addhl -group sh-highlight regex (["'])(?:\\\1|.)*?\1 0:string
    addhl -group sh-highlight regex \$(\w+|\{.+?\}) 0:identifier
~

hook global WinSetOption filetype=(?!sh).* %{
    rmhl sh-highlight
}
