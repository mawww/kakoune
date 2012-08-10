def -env-params make %{ echo make in progress, please wait...; %sh{
     output=$(mktemp -t kak-make.XXXXXXXX)
     make ${kak_param_0} ${kak_param_1} ${kak_param_2} ${kak_param_3} ${kak_param_4} >& ${output}
     echo "echo
           try %{ db *make* } catch %{ }
           edit -scratch %{*make*}
           setb filetype make
           exec %{|cat ${output}<ret>gg}
           %sh{ rm ${output} }"
}}

hook global WinSetOption filetype=make %{
    addhl group make-highlight
    addhl -group make-highlight regex "^([^:]+):(\d+):(\d+):\h+(?:(error)|(warning)|(note)|(required from(?: here)?))?[^\n]*" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make-highlight; }

def errjump %{ exec 'xs^([^:]+):(\d+)(?::(\d+))?:([^\n]+)\n<ret>'; %sh{ echo "edit ${kak_reg_1} ${kak_reg_2} ${kak_reg_3}; echo %{ ${kak_reg_4} }" } }
