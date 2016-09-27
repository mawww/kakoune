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
    printf %s\\n "hook global WinSetOption filetype=kak %{
        set window static_words '${keywords}:${values}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /kakrc/code regex \b(${keywords})\b 0:keyword
        addhl -group /kakrc/code regex \b(${values})\b 0:value
    "
}

addhl -group /kakrc/code regex \brgb:[0-9a-fA-F]{6}\b 0:value
addhl -group /kakrc/code regex (?:\bhook)\h+(?:-group\h+\S+\h+)?(?:(global|buffer|window)|(\S+))\h+(\S+) 1:attribute 2:error 3:identifier
addhl -group /kakrc/code regex (?:\bset)\h+(?:-add)?\h+(?:(global|buffer|window)|(\S+))\h+(\S+) 1:attribute 2:error 3:identifier
addhl -group /kakrc/code regex (?:\bmap)\h+(?:(global|buffer|window)|(\S+))\h+(?:(normal|insert|menu|prompt|goto|view|user|object)|(\S+))\h+(\S+) 1:attribute 2:error 3:attribute 4:error 5:identifier

addhl -group /kakrc/double_string fill string
addhl -group /kakrc/single_string fill string
addhl -group /kakrc/comment fill comment
addhl -group /kakrc/shell ref sh

hook -group kak-highlight global WinSetOption filetype=kak %{ addhl ref kakrc }
hook global WinSetOption filetype=(?!kak).* %{ rmhl kakrc }
