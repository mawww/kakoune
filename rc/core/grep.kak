declare-option -docstring "shell command run to search for subtext in a file/directory" \
    str grepcmd 'grep -RHn'
declare-option -docstring "name of the client in which utilities display information" \
    str toolsclient
declare-option -hidden int grep_current_line 0

define-command -params .. -file-completion \
    -docstring %{grep [<arguments>]: grep utility wrapper
All the optional arguments are forwarded to the grep utility} \
    grep %{ evaluate-commands %sh{
     output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-grep.XXXXXXXX)/fifo
     mkfifo ${output}
     if [ $# -gt 0 ]; then
         ( ${kak_opt_grepcmd} "$@" | tr -d '\r' > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &
     else
         ( ${kak_opt_grepcmd} "${kak_selection}" | tr -d '\r' > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &
     fi

     printf %s\\n "evaluate-commands -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} -scroll *grep*
               set-option buffer filetype grep
               set-option buffer grep_current_line 0
               hook -always -group fifo buffer BufCloseFifo .* %{
                   nop %sh{ rm -r $(dirname ${output}) }
                   remove-hooks buffer fifo
               }
           }"
}}

hook -group grep-highlight global WinSetOption filetype=grep %{
    add-highlighter window group grep
    add-highlighter window/grep regex "^((?:\w:)?[^:\n]+):(\d+):(\d+)?" 1:cyan 2:green 3:green
    add-highlighter window/grep line %{%opt{grep_current_line}} default+b
}

hook global WinSetOption filetype=grep %{
    hook buffer -group grep-hooks NormalKey <ret> grep-jump
}

hook -group grep-highlight global WinSetOption filetype=(?!grep).* %{ remove-highlighter window/grep }

hook global WinSetOption filetype=(?!grep).* %{
    remove-hooks buffer grep-hooks
}

declare-option -docstring "name of the client in which all source code jumps will be executed" \
    str jumpclient

define-command -hidden grep-jump %{
    evaluate-commands %{ # use evaluate-commands to ensure jumps are collapsed
        try %{
            execute-keys '<a-x>s^((?:\w:)?[^:]+):(\d+):(\d+)?<ret>'
            set-option buffer grep_current_line %val{cursor_line}
            evaluate-commands -try-client %opt{jumpclient} edit -existing %reg{1} %reg{2} %reg{3}
            try %{ focus %opt{jumpclient} }
        }
    }
}

define-command grep-next-match -docstring 'Jump to the next grep match' %{
    evaluate-commands -try-client %opt{jumpclient} %{
        buffer '*grep*'
        # First jump to enf of buffer so that if grep_current_line == 0
        # 0g<a-l> will be a no-op and we'll jump to the first result.
        # Yeah, thats ugly...
        execute-keys "ge %opt{grep_current_line}g<a-l> /^[^:]+:\d+:<ret>"
        grep-jump
    }
    try %{ evaluate-commands -client %opt{toolsclient} %{ execute-keys gg %opt{grep_current_line}g } }
}

define-command grep-previous-match -docstring 'Jump to the previous grep match' %{
    evaluate-commands -try-client %opt{jumpclient} %{
        buffer '*grep*'
        # See comment in grep-next-match
        execute-keys "ge %opt{grep_current_line}g<a-h> <a-/>^[^:]+:\d+:<ret>"
        grep-jump
    }
    try %{ evaluate-commands -client %opt{toolsclient} %{ execute-keys gg %opt{grep_current_line}g } }
}
