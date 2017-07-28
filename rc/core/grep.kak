decl -docstring "shell command run to search for subtext in a file/directory" \
    str grepcmd 'grep -RHn'
decl -docstring "name of the client in which utilities display information" \
    str toolsclient
decl -hidden int grep_current_line 0

def -params .. -file-completion \
    -docstring %{grep [<arguments>]: grep utility wrapper
All the optional arguments are forwarded to the grep utility} \
    grep %{ %sh{
     output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-grep.XXXXXXXX)/fifo
     mkfifo ${output}
     if [ $# -gt 0 ]; then
         ( ${kak_opt_grepcmd} "$@" | tr -d '\r' > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &
     else
         ( ${kak_opt_grepcmd} "${kak_selection}" | tr -d '\r' > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &
     fi

     printf %s\\n "eval -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} -scroll *grep*
               set buffer filetype grep
               set buffer grep_current_line 0
               hook -group fifo buffer BufCloseFifo .* %{
                   nop %sh{ rm -r $(dirname ${output}) }
                   remove-hooks buffer fifo
               }
           }"
}}

hook -group grep-highlight global WinSetOption filetype=grep %{
    add-highlighter group grep
    add-highlighter -group grep regex "^((?:\w:)?[^:]+):(\d+):(\d+)?" 1:cyan 2:green 3:green
    add-highlighter -group grep line %{%opt{grep_current_line}} default+b
}

hook global WinSetOption filetype=grep %{
    hook buffer -group grep-hooks NormalKey <ret> grep-jump
}

hook -group grep-highlight global WinSetOption filetype=(?!grep).* %{ remove-highlighter grep }

hook global WinSetOption filetype=(?!grep).* %{
    remove-hooks buffer grep-hooks
}

decl -docstring "name of the client in which all source code jumps will be executed" \
    str jumpclient

def -hidden grep-jump %{
    eval -collapse-jumps %{
        try %{
            exec '<a-x>s^((?:\w:)?[^:]+):(\d+):(\d+)?<ret>'
            set buffer grep_current_line %val{cursor_line}
            eval -try-client %opt{jumpclient} edit -existing %reg{1} %reg{2} %reg{3}
            try %{ focus %opt{jumpclient} }
        }
    }
}

def grep-next-match -docstring 'Jump to the next grep match' %{
    eval -collapse-jumps -try-client %opt{jumpclient} %{
        buffer '*grep*'
        # First jump to enf of buffer so that if grep_current_line == 0
        # 0g<a-l> will be a no-op and we'll jump to the first result.
        # Yeah, thats ugly...
        exec "ge %opt{grep_current_line}g<a-l> /^[^:]+:\d+:<ret>"
        grep-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec gg %opt{grep_current_line}g } }
}

def grep-previous-match -docstring 'Jump to the previous grep match' %{
    eval -collapse-jumps -try-client %opt{jumpclient} %{
        buffer '*grep*'
        # See comment in grep-next-match
        exec "ge %opt{grep_current_line}g<a-h> <a-/>^[^:]+:\d+:<ret>"
        grep-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec gg %opt{grep_current_line}g } }
}
