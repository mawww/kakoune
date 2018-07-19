hook global BufCreate .+\.eml %{
    set-option buffer filetype mail
}

add-highlighter shared/mail group
add-highlighter shared/mail/ regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To|Date):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
add-highlighter shared/mail/ regex <[^@>]+@.*?> 0:string
add-highlighter shared/mail/ regex ^>.*?$ 0:comment

hook -group mail-highlight global WinSetOption filetype=mail %{ add-highlighter window/mail ref mail }
hook -group mail-highlight global WinSetOption filetype=(?!mail).* %{ remove-highlighter window/mail }
