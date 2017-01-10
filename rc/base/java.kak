hook global BufCreate .*\.java %{
    set buffer filetype java
}

add-highlighter -group / regions -default code java \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ ''

add-highlighter -group /java/string fill string
add-highlighter -group /java/comment fill comment

add-highlighter -group /java/code regex %{\b(this|true|false|null)\b} 0:value
add-highlighter -group /java/code regex "\b(void|int|char|unsigned|float|boolean|double)\b" 0:type
add-highlighter -group /java/code regex "\b(while|for|if|else|do|static|switch|case|default|class|interface|goto|break|continue|return|import|try|catch|throw|new|package|extends|implements)\b" 0:keyword
add-highlighter -group /java/code regex "\b(final|public|protected|private|abstract)\b" 0:attribute

hook -group java-highlight global WinSetOption filetype=java %{ add-highlighter ref java }
hook -group java-highlight global WinSetOption filetype=(?!java).* %{ remove-highlighter java }
