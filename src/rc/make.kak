decl str makecmd make

def -shell-params make %{ %sh{
     output=$(mktemp -d -t kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( ${kak_opt_makecmd} $@ >& ${output} ) >& /dev/null < /dev/null &

     if [[ -n "$kak_opt_toolsclient" ]]; then echo "eval -client '$kak_opt_toolsclient' %{"; fi

     echo "try %{ db *make* } catch %{}
           edit -fifo ${output} *make*
           setb filetype make
           hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output}) } }"

     if [[ -n "$kak_opt_toolsclient" ]]; then echo "}"; fi
}}

hook global WinSetOption filetype=make %{
    addhl group make-highlight
    addhl -group make-highlight regex "^([^:\n]+):(\d+):(\d+):\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
    hook buffer NormalKey <c-m> errjump
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make-highlight; }

def errjump %{ exec 'xs^([^:\n]+):(\d+)(?::(\d+))?:(.*?)$<ret><a-h><space>'; edit %reg{1} %reg{2} %reg{3}; echo %reg{4} }
