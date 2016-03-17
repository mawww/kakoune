hook global BufCreate (.*/)?(kakrc|.*.kak) %{
    set buffer filetype kak
}

addhl -group / regions -default code kakrc \
    comment (^|\h)\K\# $ '' \
    double_string %{(^|\h)"} %{(?<!\\)(\\\\)*"} '' \
    single_string %{(^|\h)'} %{(?<!\\)(\\\\)*'} '' \
    shell '%sh\{' '\}' '\{'

%sh{
    # Grammar
    keywords="hook|rmhooks|addhl|rmhl|exec|eval|source|runtime|def|alias"
    keywords="${keywords}|unalias|decl|echo|edit|set|map|face|prompt|menu|info"
    keywords="${keywords}|try|catch|nameclient|namebuf|cd|colorscheme"
    values="default|black|red|green|yellow|blue|magenta|cyan|white"

    # Add the language's grammar to the static completion list
    printf %s "hook global WinSetOption filetype=kak %{
        set window static_words '${keywords}'
        set -add window static_words '${values}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /kakrc/code regex \<(${keywords})\> 0:keyword
        addhl -group /kakrc/code regex \<(${values})\> 0:value
    "
}

addhl -group /kakrc/code regex \<rgb:[0-9a-fA-F]{6}\> 0:value
addhl -group /kakrc/code regex (?:\<hook)\h+(?:(global|buffer|window)|(\S+))\h+(\S+)\h+(\S+)? 1:attribute 2:error 3:identifier 4:string
addhl -group /kakrc/code regex (?:\<set)\h+(?:(global|buffer|window)|(\S+))\h+(\S+)\h+(\S+)? 1:attribute 2:error 3:identifier 4:string
addhl -group /kakrc/code regex (?:\<map)\h+(?:(global|buffer|window)|(\S+))\h+(?:(normal|insert|prompt|menu)|(\S+))\h+(\S+)\h+(\S+)? 1:attribute 2:error 3:attribute 4:error 5:identifier 6:string

addhl -group /kakrc/double_string fill string
addhl -group /kakrc/single_string fill string
addhl -group /kakrc/comment fill comment
addhl -group /kakrc/shell ref sh

hook global WinSetOption filetype=kak %{ addhl ref kakrc }
hook global WinSetOption filetype=(?!kak).* %{ rmhl kakrc }
