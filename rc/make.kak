decl str makecmd make
decl str toolsclient
decl -hidden int _make_current_error_line

def -shell-params make %{ %sh{
     output=$(mktemp -d -t kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( eval ${kak_opt_makecmd} "$@" > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &

     echo "eval -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} -scroll *make*
               set buffer filetype make
               set buffer _make_current_error_line 0
               hook buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
           }"
}}

addhl -group / group make
addhl -group /make regex "^([^:\n]+):(\d+):(\d+):\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow

hook global WinSetOption filetype=make %{
    addhl ref make
    hook buffer -group make-hooks NormalKey <c-m> errjump
}

hook global WinSetOption filetype=(?!make).* %{ rmhl make; rmhooks buffer make-hooks }

decl str jumpclient

def errjump -docstring 'Jump to error location' %{
    try %{
        exec gll<a-?> "Entering directory" <ret>
        exec s "Entering directory '([^']+)'.*\n([^:]+):(\d+):(\d+):([^\n]+)\'" <ret>l
        set buffer _make_current_error_line %val{cursor_line}
        eval -try-client %opt{jumpclient} %rec{edit %reg{1}/%reg{2} %reg{3} %reg{4}; echo -color Information '%reg{5}'}
        try %{ focus %opt{jumpclient} }
    } catch %{
        exec ghgl s "([^:]+):(\d+):(\d+):([^\n]+)\'" <ret>l
        set buffer _make_current_error_line %val{cursor_line}
        eval -try-client %opt{jumpclient} %rec{edit %reg{1} %reg{2} %reg{3}; echo -color Information '%reg{4}'}
        try %{ focus %opt{jumpclient} }
    }
}

def errnext -docstring 'Jump to next error' %{
    eval -try-client %opt{jumpclient} %{
        buffer '*make*'
        exec %rec{%opt{_make_current_error_line}ggl/[0-9]+: error:<ret>}
        errjump
    }
}

def errprev -docstring 'Jump to previous error' %{
    eval -try-client %opt{jumpclient} %{
        buffer '*make*'
        exec %rec{%opt{_make_current_error_line}ggh<a-/>[0-9]+: error:<ret>}
        errjump
    }
}
