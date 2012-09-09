def -shell-params grep %{ echo grep in progress, please wait...; %sh{
     output=$(mktemp -t kak-grep.XXXXXXXX)
     grep -PHn $@ >& ${output} < /dev/null &
     echo "echo
           try %{ db *grep* } catch %{ }
           edit -fifo ${output} *grep*
           setb filetype grep
           hook buffer BufClose .* %{ %sh{ rm ${output} } }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep-highlight
    addhl -group grep-highlight regex "^([^:]+):(\d+):" 1:cyan 2:green
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep-highlight; }

def gjump %{ exec 'xs^([^:]+):(\d+)<ret>'; edit %reg{1} %reg{2} }
