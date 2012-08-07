def -env-params grep %{ echo grep in progress, please wait...; %sh{
     output=$(mktemp -t kak-grep.XXXXXXXX)
     grep -Hn $kak_param0 $kak_param1 $kak_param2 $kak_param3 $kak_param4 >& ${output}
     echo "echo
           try %{ db *grep* } catch %{ }
           edit -scratch *grep*
           setb filetype grep
           exec %{|cat ${output}<ret>gg}
           %sh{rm ${output} }"
}}

hook global WinSetOption filetype=grep %{
    addhl group grep-highlight
    addhl -group grep-highlight regex "^([^:]+):(\d+):" 1:cyan 2:green
}

hook global WinSetOption filetype=(?!grep).* %{ rmhl grep-highlight; }

def gjump %{ exec 'xs^([^:]+):(\d+)<ret>'; edit %sh{ echo ${kak_reg_1} ${kak_reg_2} } }
