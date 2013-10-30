hook global BufOpen .* %{ %sh{
     mimetype="$(file -b --mime-type ${kak_bufname})"
     if [[ "${mimetype}" == "message/rfc822" ]]; then
         echo set buffer filetype mail;
     fi
} }

hook global WinSetOption filetype=mail %~
    addhl group mail-highlight
    addhl -group mail-highlight regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
    addhl -group mail-highlight regex <[^@>]+@.*?> 0:string
    addhl -group mail-highlight regex ^>.*?$ 0:comment
~

hook global WinSetOption filetype=(?!mail).* %{
    rmhl mail-highlight
}
