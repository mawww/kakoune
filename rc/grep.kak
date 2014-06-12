decl str grepcmd 'grep -RHn'
decl str toolsclient

def -shell-params -file-completion \
    grep %{ %sh{
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
               hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output}) } }
           }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep
    addhl -group grep regex "^([^:]+):(\d+):(\d+)?" 1:cyan 2:green 3:green
    hook buffer -id grep-hooks NormalKey <c-m> jump
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep; rmhooks buffer grep-hooks }

decl str jumpclient

def jump %{
    exec 'xs^([^:]+):(\d+):(\d+)?<ret>'
    eval -try-client %opt{jumpclient} edit %reg{1} %reg{2} %reg{3}
    try %{ focus %opt{jumpclient} }
}
