decl str makecmd make
decl str toolsclient

def -shell-params make %{ %sh{
     output=$(mktemp -d -t kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( eval ${kak_opt_makecmd} "$@" > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &

     echo "eval -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} *make*
               set buffer filetype make
               hook buffer BufClose .* %{ nop %sh{ rm -r $(dirname ${output}) } }
           }"
}}

defhl make
addhl -def-group make regex "^([^:\n]+):(\d+):(\d+):\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow

hook global WinSetOption filetype=make %{
    addhl ref make
    hook buffer -id make-hooks NormalKey <c-m> errjump
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make; rmhooks buffer make-hooks }

def errjump -docstring 'Jump to error location' %{
    try %{
        exec gll<a-?> "Entering directory" <ret>
        exec s "Entering directory '([^']+)'.*\n([^:]+):(\d+):(\d+):[^\n]+\'" <ret>l
        edit %rec{%reg{1}/%reg{2}} %reg{3} %reg{4}
    } catch %{
        exec ghgl s "([^:]+):(\d+):(\d+):[^\n]+\'" <ret>l
        edit %reg{1} %reg{2} %reg{3}
    }
}
