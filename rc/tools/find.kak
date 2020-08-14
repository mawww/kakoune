declare-option -docstring "shell command run to search for file in a directory" \
    str findcmd 'find'
declare-option -docstring "name of the client in which utilities display information" \
    str toolsclient
declare-option -hidden int find_current_line 0

define-command -params .. -file-completion \
    -docstring %{find [<arguments>]: find utility wrapper
All the optional arguments are forwarded to the find utility} \
    find %{ evaluate-commands %sh{
     if [ $# -eq 0 ]; then
         set -- "${kak_selection}"
     fi

     output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-find.XXXXXXXX)/fifo
     mkfifo ${output}
     ( ${kak_opt_findcmd} "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

     printf %s\\n "evaluate-commands -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} *find*
               set-option buffer filetype find
               set-option buffer find_current_line 0
               hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
           }"
}}

hook -group find-highlight global WinSetOption filetype=find %{
    add-highlighter window/find group
    add-highlighter window/find/ regex "^([^\n]+)" 1:cyan
    add-highlighter window/find/ line %{%opt{find_current_line}} default+b
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/find}
}

hook global WinSetOption filetype=find %{
    hook buffer -group find-hooks NormalKey <ret> find-jump
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks buffer find-hooks }
}

declare-option -docstring "name of the client in which all source code jumps will be executed" \
    str jumpclient

define-command -hidden find-jump %{
    evaluate-commands %{ # use evaluate-commands to ensure jumps are collapsed
        try %{
            execute-keys '<a-x>H<ret>'
            set-option buffer find_current_line %val{cursor_line}
            evaluate-commands -try-client %opt{jumpclient} -verbatim -- edit -existing %val{selection}
            try %{ focus %opt{jumpclient} }
        }
    }
}

define-command find-next-match -docstring 'Jump to the next find match' %{
    evaluate-commands -try-client %opt{jumpclient} %{
        buffer '*find*'
        # First jump to enf of buffer so that if find_current_line == 0
        # 0g<a-l> will be a no-op and we'll jump to the first result.
        # Yeah, thats ugly...
        execute-keys "ge %opt{find_current_line}g<a-l> /^[^\n]+<ret>"
        find-jump
    }
    try %{ evaluate-commands -client %opt{toolsclient} %{ execute-keys gg %opt{find_current_line}g } }
}

define-command find-previous-match -docstring 'Jump to the previous find match' %{
    evaluate-commands -try-client %opt{jumpclient} %{
        buffer '*find*'
        # See comment in find-next-match
        execute-keys "ge %opt{find_current_line}g<a-h> <a-/>^[^\n]+<ret>"
        find-jump
    }
    try %{ evaluate-commands -client %opt{toolsclient} %{ execute-keys gg %opt{find_current_line}g } }
}
