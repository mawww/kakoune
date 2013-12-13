decl str grepcmd 'grep -RHn'
decl str toolsclient

def -shell-params -file-completion \
    grep %{ %sh{
     output=$(mktemp -d -t kak-grep.XXXXXXXX)/fifo
     mkfifo ${output}
     if (( $# > 0 )); then
         ( ${kak_opt_grepcmd} "$@" | tr -d '\r' >& ${output} ) >& /dev/null < /dev/null &
     else
         ( ${kak_opt_grepcmd} "${kak_selection}" | tr -d '\r' >& ${output} ) >& /dev/null < /dev/null &
     fi

     echo "eval -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} *grep*
               set buffer filetype grep
               hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output}) } }
           }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep-highlight
    addhl -group grep-highlight regex "^([^:]+):(\d+):(\d+)?" 1:cyan 2:green 3:green
    hook buffer -id grep-hooks NormalKey <c-m> jump
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep-highlight; rmhooks buffer grep-hooks }

decl str jumpclient

def jump %{
    exec 'xs^([^:]+):(\d+):(\d+)?<ret>'
    eval -try-client %opt{jumpclient} edit %reg{1} %reg{2} %reg{3}
}
