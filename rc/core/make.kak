decl str makecmd make
decl str make_error_pattern " (?:fatal )?error:"

decl str toolsclient
decl -hidden int make_current_error_line

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
               set buffer make_current_error_line 0
               hook -group fifo buffer BufCloseFifo .* %{
                   nop %sh{ rm -r $(dirname ${output}) }
                   remove-hooks buffer fifo
               }
           }"
}}

add-highlighter -group / group make
add-highlighter -group /make regex "^((?:\w:)?[^:\n]+):(\d+):(?:(\d+):)?\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
add-highlighter -group /make regex "^\h*(~*(?:(\^)~*)?)$" 1:green 2:cyan+b
add-highlighter -group /make line '%opt{make_current_error_line}' default+b

hook -group make-highlight global WinSetOption filetype=make %{ add-highlighter ref make }

hook global WinSetOption filetype=make %{
    hook buffer -group make-hooks NormalKey <ret> make-jump
}

hook -group make-highlight global WinSetOption filetype=(?!make).* %{ remove-highlighter make }

hook global WinSetOption filetype=(?!make).* %{
    remove-hooks buffer make-hooks
}

decl str jumpclient

def -hidden make-jump %{
    eval -collapse-jumps %{
        try %{
            exec gl<a-?> "Entering directory" <ret><a-:>
            # Try to parse the error into capture groups, failing on absolute paths
            exec s "Entering directory '([^']+)'.*\n([^:/][^:]*):(\d+):(?:(\d+):)?([^\n]+)\'" <ret>l
            set buffer make_current_error_line %val{cursor_line}
            eval -try-client %opt{jumpclient} "edit -existing %reg{1}/%reg{2} %reg{3} %reg{4}; echo -color Information %{%reg{5}}; try %{ focus }"
        } catch %{
            exec <a-h><a-l> s "((?:\w:)?[^:]+):(\d+):(?:(\d+):)?([^\n]+)\'" <ret>l
            set buffer make_current_error_line %val{cursor_line}
            eval -try-client %opt{jumpclient} "edit -existing %reg{1} %reg{2} %reg{3}; echo -color Information %{%reg{4}}; try %{ focus }"
        }
    }
}

def make-next -docstring 'Jump to the next make error' %{
    eval -collapse-jumps -try-client %opt{jumpclient} %{
        buffer '*make*'
        exec "%opt{make_current_error_line}ggl" "/^(?:\w:)?[^:\n]+:\d+:(?:\d+:)?%opt{make_error_pattern}<ret>"
        make-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec %opt{make_current_error_line}g } }
}

def make-prev -docstring 'Jump to the previous make error' %{
    eval -collapse-jumps -try-client %opt{jumpclient} %{
        buffer '*make*'
        exec "%opt{make_current_error_line}g" "<a-/>^(?:\w:)?[^:\n]+:\d+:(?:\d+:)?%opt{make_error_pattern}<ret>"
        make-jump
    }
    try %{ eval -client %opt{toolsclient} %{ exec %opt{make_current_error_line}g } }
}
