declare-option -docstring "shell command run to build the project" \
    str makecmd make
declare-option -docstring "pattern that describes lines containing information about errors in the output of the `makecmd` command" \
    str make_error_pattern " (?:fatal )?error:"

declare-option -docstring "name of the client in which utilities display information" \
    str toolsclient
declare-option -hidden int make_current_error_line

define-command -params .. \
    -docstring %{
        make [<arguments>]: make utility wrapper
        All the optional arguments are forwarded to the make utility
     } make %{ evaluate-commands %sh{
     output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-make.XXXXXXXX)/fifo
     mkfifo ${output}
     ( eval "${kak_opt_makecmd}" "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

     printf %s\\n "evaluate-commands -try-client '$kak_opt_toolsclient' %{
               edit! -fifo ${output} -scroll *make*
               set-option buffer filetype make
               set-option buffer make_current_error_line 0
               hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
           }"
}}

add-highlighter shared/make group
add-highlighter shared/make/ regex "^((?:\w:)?[^:\n]+):(\d+):(?:(\d+):)?\h+(?:((?:fatal )?error)|(warning)|(note)|(required from(?: here)?))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow 6:blue 7:yellow
add-highlighter shared/make/ regex "^\h*(~*(?:(\^)~*)?)$" 1:green 2:cyan+b
add-highlighter shared/make/ line '%opt{make_current_error_line}' default+b

hook -group make-highlight global WinSetOption filetype=make %{
    add-highlighter window/make ref make
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/make }
}

hook global WinSetOption filetype=make %{
    hook buffer -group make-hooks NormalKey <ret> make-jump
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks buffer make-hooks }
}

declare-option -docstring "name of the client in which all source code jumps will be executed" \
    str jumpclient

define-command -hidden make-open-error -params 4 %{
    evaluate-commands -try-client %opt{jumpclient} %{
        edit -existing "%arg{1}" %arg{2} %arg{3}
        echo -markup "{Information}{\}%arg{4}"
        try %{ focus }
    }
}

define-command -hidden make-jump %{
    evaluate-commands %{
        try %{
            execute-keys gl<a-?> "Entering directory" <ret><a-:>
            # Try to parse the error into capture groups, failing on absolute paths
            execute-keys s "Entering directory [`']([^']+)'.*\n([^:/][^:]*):(\d+):(?:(\d+):)?([^\n]+)\z" <ret>l
            set-option buffer make_current_error_line %val{cursor_line}
            make-open-error "%reg{1}/%reg{2}" "%reg{3}" "%reg{4}" "%reg{5}"
        } catch %{
            execute-keys <a-h><a-l> s "((?:\w:)?[^:]+):(\d+):(?:(\d+):)?([^\n]+)\z" <ret>l
            set-option buffer make_current_error_line %val{cursor_line}
            make-open-error "%reg{1}" "%reg{2}" "%reg{3}" "%reg{4}"
        }
    }
}

define-command make-next-error -docstring 'Jump to the next make error' %{
    evaluate-commands -try-client %opt{jumpclient} %{
        buffer '*make*'
        execute-keys "%opt{make_current_error_line}ggl" "/^(?:\w:)?[^:\n]+:\d+:(?:\d+:)?%opt{make_error_pattern}<ret>"
        make-jump
    }
    try %{
        evaluate-commands -client %opt{toolsclient} %{
            buffer '*make*'
            execute-keys %opt{make_current_error_line}g
        }
    }
}

define-command make-previous-error -docstring 'Jump to the previous make error' %{
    evaluate-commands -try-client %opt{jumpclient} %{
        buffer '*make*'
        execute-keys "%opt{make_current_error_line}g" "<a-/>^(?:\w:)?[^:\n]+:\d+:(?:\d+:)?%opt{make_error_pattern}<ret>"
        make-jump
    }
    try %{
        evaluate-commands -client %opt{toolsclient} %{
            buffer '*make*'
            execute-keys %opt{make_current_error_line}g
        }
    }
}
