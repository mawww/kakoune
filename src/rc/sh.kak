hook global BufCreate .*\.(sh) %{
    setb filetype sh
}

hook global BufOpen .* %{ %sh{
     mimetype="$(file -b --mime-type ${kak_bufname})"
     if [[ "${mimetype}" == "text/x-shellscript" ]]; then
         echo setb filetype sh;
     fi
} }

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
