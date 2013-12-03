hook global BufSetOption mimetype=message/rfc822 %{ set buffer filetype mail }

defhl mail
addhl -def-group mail regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
addhl -def-group mail regex <[^@>]+@.*?> 0:string
addhl -def-group mail regex ^>.*?$ 0:comment

hook global WinSetOption filetype=mail %{ addhl ref mail }
hook global WinSetOption filetype=(?!mail).* %{ rmhl mail }
