hook global BufCreate .+\.eml %{
    set buffer filetype mail
}

add-highlighter -group / group mail
add-highlighter -group /mail regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
add-highlighter -group /mail regex <[^@>]+@.*?> 0:string
add-highlighter -group /mail regex ^>.*?$ 0:comment

hook -group mail-highlight global WinSetOption filetype=mail %{ add-highlighter ref mail }
hook -group mail-highlight global WinSetOption filetype=(?!mail).* %{ remove-highlighter mail }
