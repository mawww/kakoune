def -shell-params make %{ %sh{
     output=$(mktemp -d -t kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( make $@ >& ${output} ) >& /dev/null < /dev/null &
     echo "try %{ db *make* } catch %{ }
           edit -fifo ${output} *make*
           setb filetype make
           hook buffer BufClose .* %{ %sh{ rm -r $(dirname ${output}) } }"
}}

hook global WinSetOption filetype=make %{
    addhl group make-highlight
    addhl -group make-highlight regex "^([^:\n]+):(\d+):(\d+):\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make-highlight; }

def errjump %{ exec 'xs^([^:\n]+):(\d+)(?::(\d+))?:(.*?)$<ret>'; edit %reg{1} %reg{2} %reg{3}; echo %reg{4} }
