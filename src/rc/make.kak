def -env-params make %{ echo make in progress, please wait...; %sh{
     output=$(mktemp -t kak-make.XXXXXXXX)
     make $kak_param_0 $kak_param_1 $kak_param_2 $kak_param_3 $kak_param_4 >& ${output} < /dev/null &
     echo "echo
           try %{ db *make* } catch %{ }
           edit -fifo ${output} *make*
           setb filetype make
           hook buffer BufClose .* %{ %sh{ rm ${output} } }"
}}

hook global WinSetOption filetype=make %{
    addhl group make-highlight
    addhl -group make-highlight regex "^([^:]+):(\d+):(\d+):\h+(?:(error)|(warning)|(note)|(required from(?: here)?))?[^\n]*" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make-highlight; }

def errjump %{ exec 'xs^([^:]+):(\d+)(?::(\d+))?:([^\n]+)\n<ret>'; edit %reg{1} %reg{2} %reg{3}; echo %reg{4}" }
