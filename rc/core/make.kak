decl str makecmd make
decl str make_error_pattern " (?:fatal )?error:"

decl str toolsclient
decl -hidden int _make_current_error_line

def -params .. \
    -docstring %{make [<arguments>]: make utility wrapper
All the optional arguments are forwarded to the make utility} \
    make %{ %sh{
     output=$(mktemp -d -t kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( eval ${kak_opt_makecmd} "$@" > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &

     printf %s\\n "eval -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} -scroll *make*
               set buffer filetype make
               set buffer _make_current_error_line 0
               hook -group fifo buffer BufCloseFifo .* %{
                   nop %sh{ rm -r $(dirname ${output}) }
                   rmhooks buffer fifo
               }
           }"
}}

addhl -group / group make
addhl -group /make regex "^((?:\w:)?[^:\n]+):(\d+):(?:(\d+):)?\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
addhl -group /make line %{%opt{_make_current_error_line}} default+b

hook -group make-highlight global WinSetOption filetype=make %{ addhl ref make }

hook global WinSetOption filetype=make %{
    hook buffer -group make-hooks NormalKey <ret> make-jump
}

hook -group make-highlight global WinSetOption filetype=(?!make).* %{ rmhl make }

hook global WinSetOption filetype=(?!make).* %{
    rmhooks buffer make-hooks
}

decl str jumpclient

def -hidden make-jump %{
    eval -collapse-jumps %{
        try %{
            exec gl<a-?> "Entering directory" <ret>
            exec s "Entering directory '([^']+)'.*\n([^:]+):(\d+):(?:(\d+):)?([^\n]+)\'" <ret>l
            set buffer _make_current_error_line %val{cursor_line}
            eval -try-client %opt{jumpclient} "edit -existing %reg{1}/%reg{2} %reg{3} %reg{4}; echo -color Information %{%reg{5}}"
            try %{ focus %opt{jumpclient} }
        } catch %{
            exec <a-h><a-l> s "((?:\w:)?[^:]+):(\d+):(?:(\d+):)?([^\n]+)\'" <ret>l
            set buffer _make_current_error_line %val{cursor_line}
            eval -try-client %opt{jumpclient} "edit -existing %reg{1} %reg{2} %reg{3}; echo -color Information %{%reg{4}}"
            try %{ focus %opt{jumpclient} }
        }
    }
}

def make-next -docstring 'Jump to the next make error' %{
    eval -collapse-jumps -try-client %opt{jumpclient} %{
        buffer '*make*'
        exec "%opt{_make_current_error_line}g<a-l>/(?:\w:)?[^:]+:\d+:(?:\d+:)?%opt{make_error_pattern}<ret>"
        make-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec %opt{_make_current_error_line}g } }
}

def make-prev -docstring 'Jump to the previous make error' %{
    eval -collapse-jumps -try-client %opt{jumpclient} %{
        buffer '*make*'
        exec "%opt{_make_current_error_line}g<a-h><a-/>(?:\w:)?[^:]+:\d+:(?:\d+:)?%opt{make_error_pattern}<ret>"
        make-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec %opt{_make_current_error_line}g } }
}
