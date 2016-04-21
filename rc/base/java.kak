hook global BufCreate .*\.java %{
    set buffer filetype java
}

hook global BufSetOption mimetype=text/java %{
    set buffer filetype java
}

addhl -group / regions -default code java \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ ''

addhl -group /java/string fill string
addhl -group /java/comment fill comment

addhl -group /java/code regex %{\b(this|true|false|null)\b} 0:value
addhl -group /java/code regex "\b(void|int|char|unsigned|float|boolean|double)\b" 0:type
addhl -group /java/code regex "\b(while|for|if|else|do|static|switch|case|default|class|interface|goto|break|continue|return|import|try|catch|throw|new|package|extends|implements)\b" 0:keyword
addhl -group /java/code regex "\b(final|public|protected|private|abstract)\b" 0:attribute

hook global WinSetOption filetype=java %{
    addhl ref java
}

hook global WinSetOption filetype=(?!java).* %{
    rmhl java
}
