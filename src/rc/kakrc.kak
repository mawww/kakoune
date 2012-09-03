hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    setb filetype kak
}

hook global WinSetOption filetype=kak %{
    addhl group kak-highlight
    addhl -group kak-highlight regex \<(hook|addhl|rmhl|addfilter|rmfilter|exec|source|runtime|def|echo|edit|set[gbw])\> 0:green
    addhl -group kak-highlight regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> 0:yellow
    addhl -group kak-highlight regex (?<=\<hook)\h+((global|buffer|window)|(\S+))\h+(\S+)\h+(\H+) 2:blue 3:red 4:cyan 5:magenta
    addhl -group kak-highlight regex (?<=\<regex)\h+(\S+) 1:magenta
    addhl -group kak-highlight regex (["'])(?:\\\1|.)*?\1 0:magenta
    addhl -group kak-highlight regex (?<=\<set[gbw])\h+(\S+)\h+(\S+) 1:magenta 2:red
    addhl -group kak-highlight regex \#[^\n]*\n 0:cyan
}

hook global WinSetOption filetype=(?!kak).* %{
    rmhl kak-highlight
}
