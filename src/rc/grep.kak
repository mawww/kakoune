def -shell-params -file-completion \
    grep %{ echo grep in progress, please wait...; %sh{
     output=$(mktemp -d -t kak-grep.XXXXXXXX)/fifo
     mkfifo ${output}
     ( grep -PHn "$@" >& ${output} ) >& /dev/null < /dev/null &
     echo "echo
           try %{ db *grep* } catch %{ }
           edit -fifo ${output} *grep*
           setb filetype grep
           hook buffer BufClose .* %{ %sh{ rm -r $(dirname ${output}) } }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep-highlight
    addhl -group grep-highlight regex "^([^:]+):(\d+):" 1:cyan 2:green
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep-highlight; }

def gjump %{ exec 'xs^([^:]+):(\d+)<ret>'; edit %reg{1} %reg{2} }
