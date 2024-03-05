hook global BufCreate .+\.eml %{
    set-option buffer filetype mail
}

hook global WinSetOption filetype=mail %{
    require-module mail
    map buffer normal <ret> :diff-jump<ret>
    hook -once -always window WinSetOption filetype=.* %{
        unmap buffer normal <ret> :diff-jump<ret>
    }
}

hook -group mail-highlight global WinSetOption filetype=mail %{
    add-highlighter window/mail ref mail
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/mail }
}


provide-module mail %{

require-module diff

add-highlighter shared/mail group
add-highlighter shared/mail/ ref diff
add-highlighter shared/mail/ regex ^(From|To|Cc|Bcc|Subject|Reply-To|In-Reply-To|References|Date|Message-Id|User-Agent):([^\n]*(?:\n\h+[^\n]+)*)$ 1:keyword 2:attribute
add-highlighter shared/mail/ regex <[a-zA-Z0-9!#$%&'*+/=?^`{|}~.-]+@[a-zA-Z0-9!#$%&'*+/=?^`{|}~.-]+> 0:string
add-highlighter shared/mail/ regex ^>.*?$ 0:comment
add-highlighter shared/mail/ regex ^--\ \n.* 0:comment

}
