decl str makecmd make
decl str toolsclient

def -shell-params make %{ %sh{
     output=$(mktemp -d -t kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( eval ${kak_opt_makecmd} $@ >& ${output} ) >& /dev/null < /dev/null &

     [[ -n "$kak_opt_toolsclient" ]] && echo "eval -client '$kak_opt_toolsclient' %{"

     echo "edit! -fifo ${output} *make*
           set buffer filetype make
           hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output}) } }"

     [[ -n "$kak_opt_toolsclient" ]] && echo "}"
}}

defhl make
addhl -def-group make regex "^([^:\n]+):(\d+):(\d+):\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow

hook global WinSetOption filetype=make %{
    addhl ref make
    hook buffer -id make-hooks NormalKey <c-m> errjump
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make; rmhooks buffer make-hooks }

def errjump %{ exec 'xs^([^:\n]+):(\d+)(?::(\d+))?:(.*?)$<ret><a-h><space>'; edit %reg{1} %reg{2} %reg{3}; echo %reg{4} }
