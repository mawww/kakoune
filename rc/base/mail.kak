hook global BufSetOption mimetype=message/rfc822 %{ set buffer filetype mail }

addhl -group / group mail
addhl -group /mail regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
addhl -group /mail regex <[^@>]+@.*?> 0:string
addhl -group /mail regex ^>.*?$ 0:comment

hook -group mail-highlight global WinSetOption filetype=mail %{ addhl ref mail }
hook -group mail-highlight global WinSetOption filetype=(?!mail).* %{ rmhl mail }
