hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    set buffer filetype kak
}

addhl -group / regions -default code kakrc \
    comment (^|\h)\K\# $ '' \
    double_string %{(^|\h)"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(^|\h)'} %{(?<!\\)(\\\\)*'} '' \
    shell '%sh\{' '\}' '\{'

addhl -group /kakrc/code regex \<(hook|rmhooks|addhl|rmhl|add|exec|eval|source|runtime|def|decl|echo|edit|set)\> 0:keyword
addhl -group /kakrc/code regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> 0:value
addhl -group /kakrc/code regex (?<=\<hook)\h+((global|buffer|window)|(\S+))\h+(\S+)\h+(\H+) 2:attribute 3:error 4:identifier 5:string
addhl -group /kakrc/code regex (?<=\<set)\h+((global|buffer|window)|(\S+))\h+(\S+)\h+(\S+) 2:attribute 3:error 4:identifier 5:value
addhl -group /kakrc/code regex (?<=\<regex)\h+(\S+) 1:string

addhl -group /kakrc/double_string fill string
addhl -group /kakrc/single_string fill string
addhl -group /kakrc/comment fill comment
addhl -group /kakrc/shell ref sh

hook global WinSetOption filetype=kak %{ addhl ref kakrc }
hook global WinSetOption filetype=(?!kak).* %{ rmhl kakrc }
