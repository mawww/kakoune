hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    setb filetype kak
}

hook global WinSetOption filetype=kak %{
    addhl group kak-highlight
    addhl -group kak-highlight regex \<(hook|addhl|rmhl|addfilter|rmfilter|exec|source|runtime|def|echo|edit|set[gbw])\> 0:green,default
    addhl -group kak-highlight regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> 0:yellow,default
    addhl -group kak-highlight regex (?<=\<hook)(\h+\w+){2}\h+\H+ 0:magenta,default
    addhl -group kak-highlight regex (?<=\<hook)(\h+\w+){2} 0:cyan,default
    addhl -group kak-highlight regex (?<=\<hook)(\h+\w+) 0:red,default
    addhl -group kak-highlight regex (?<=\<hook)(\h+(global|buffer|window)) 0:blue,default
    addhl -group kak-highlight regex (?<=\<regex)\h+\S+ 0:magenta,default
    addhl -group kak-highlight regex (?<=\<set[gbw])\h+\S+\h+\S+ 0:magenta,default
    addhl -group kak-highlight regex (?<=\<set[gbw])\h+\S+ 0:red,default
}

hook global WinSetOption filetype=(?!kak).* %{
    rmhl kak-highlight
}
