hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    setb filetype kak
}

hook global WinSetOption filetype=kak %{
    addhl group kak-highlight
    addhl -group kak-highlight regex \<(hook|rmhooks|addhl|rmhl|addfilter|rmfilter|exec|eval|source|runtime|def|decl|echo|edit|set[gbw])\> 0:keyword
    addhl -group kak-highlight regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> 0:value
    addhl -group kak-highlight regex (?<=\<hook)\h+((global|buffer|window)|(\S+))\h+(\S+)\h+(\H+) 2:attribute 3:error 4:identifier 5:string
    addhl -group kak-highlight regex (?<=\<regex)\h+(\S+) 1:string
    addhl -group kak-highlight regex (["'])(?:\\\1|.)*?\1 0:string
    addhl -group kak-highlight regex (?<=\<set[gbw])\h+(\S+)\h+(\S+) 1:identifier 2:value
    addhl -group kak-highlight regex (^|\h)\#[^\n]*\n 0:comment
}

hook global WinSetOption filetype=(?!kak).* %{
    rmhl kak-highlight
}
