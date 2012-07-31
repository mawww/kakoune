hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    setb filetype kak
}

hook global WinSetOption filetype=kak %{
    addhl group kak-highlight
    addhl -group kak-highlight regex \<(hook|addhl|rmhl|addfilter|rmfilter|exec|source|runtime|def|echo|edit|set[gbw])\> green default
    addhl -group kak-highlight regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> yellow default
    addhl -group kak-highlight regex (?<=\<hook)(\h+\w+){2}\h+\H+ magenta default
    addhl -group kak-highlight regex (?<=\<hook)(\h+\w+){2} cyan default
    addhl -group kak-highlight regex (?<=\<hook)(\h+\w+) red default
    addhl -group kak-highlight regex (?<=\<hook)(\h+(global|buffer|window)) blue default
    addhl -group kak-highlight regex (?<=\<regex)\h+\S+ magenta default
    addhl -group kak-highlight regex (?<=\<set[gbw])\h+\S+\h+\S+ magenta default
    addhl -group kak-highlight regex (?<=\<set[gbw])\h+\S+ red default
}

hook global WinSetOption filetype=(?!kak).* %{
    rmhl kak-highlight
}
