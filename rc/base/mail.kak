hook global BufCreate .+\.eml %{
    set-option buffer filetype mail
}

add-highlighter shared/ group mail
add-highlighter shared/mail regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
add-highlighter shared/mail regex <[^@>]+@.*?> 0:string
add-highlighter shared/mail regex ^>.*?$ 0:comment

hook -group mail-highlight global WinSetOption filetype=mail %{ add-highlighter window ref mail }
hook -group mail-highlight global WinSetOption filetype=(?!mail).* %{ remove-highlighter window/mail }
