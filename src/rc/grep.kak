setg grepcmd 'grep -RHn'

def -shell-params -file-completion \
    grep %{ %sh{
     output=$(mktemp -d -t kak-grep.XXXXXXXX)/fifo
     mkfifo ${output}
     if (( $# > 0 )); then
         ( ${kak_opt_grepcmd} "$@" >& ${output} ) >& /dev/null < /dev/null &
     else
         ( ${kak_opt_grepcmd} "${kak_selection}" >& ${output} ) >& /dev/null < /dev/null &
     fi
     echo "try %{ db *grep* } catch %{}
           edit -fifo ${output} *grep*
           setb filetype grep
           hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output}) } }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep-highlight
    addhl -group grep-highlight regex "^([^:]+):(\d+):" 1:cyan 2:green
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep-highlight; }

def jump %{ exec 'xs^([^:]+):(\d+):(\d+)?<ret>'; edit %reg{1} %reg{2} %reg{3} }
