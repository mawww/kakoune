decl str grepcmd 'grep -RHn'
decl str toolsclient
decl -hidden int _grep_current_line 0

def -params .. -file-completion \
    grep -docstring "Grep utility wrapper" %{ %sh{
     output=$(mktemp -d -t kak-grep.XXXXXXXX)/fifo
     mkfifo ${output}
     if [ $# -gt 0 ]; then
         ( ${kak_opt_grepcmd} "$@" | tr -d '\r' > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &
     else
         ( ${kak_opt_grepcmd} "${kak_selection}" | tr -d '\r' > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &
     fi

     echo "eval -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} -scroll *grep*
               set buffer filetype grep
               set buffer _grep_current_line 0
               hook -group fifo buffer BufCloseFifo .* %{
                   nop %sh{ rm -r $(dirname ${output}) }
                   rmhooks buffer fifo
               }
           }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep
    addhl -group grep regex "^((?:\w:)?[^:]+):(\d+):(\d+)?" 1:cyan 2:green 3:green
    addhl -group grep line %{%opt{_grep_current_line}} default+b
    hook buffer -group grep-hooks NormalKey <c-m> grep-jump
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep; rmhooks buffer grep-hooks }

decl str jumpclient

def grep-jump %{
    try %{
        exec 'xs^((?:\w:)?[^:]+):(\d+):(\d+)?<ret>'
        set buffer _grep_current_line %val{cursor_line}
        eval -try-client %opt{jumpclient} edit -existing %reg{1} %reg{2} %reg{3}
        try %{ focus %opt{jumpclient} }
    }
}

def grep-next -docstring 'Jump to next grep match' %{
    eval -try-client %opt{jumpclient} %{
        buffer '*grep*'
        exec "%opt{_grep_current_line}g<a-l>/^[^:]+:\d+:<ret>"
        grep-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec %opt{_grep_current_line}g } }
}

def grep-prev -docstring 'Jump to previous grep match' %{
    eval -try-client %opt{jumpclient} %{
        buffer '*grep*'
        exec "%opt{_grep_current_line}g<a-/>^[^:]+:\d+:<ret>"
        grep-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec %opt{_grep_current_line}g } }
}
