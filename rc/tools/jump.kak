declare-option -docstring "name of the client in which all source code jumps will be executed" \
    str jumpclient
declare-option -docstring "name of the client in which utilities display information" \
    str toolsclient

provide-module jump %{

declare-option -hidden str jump_current_buffer ''
declare-option -hidden int jump_current_line 0

define-command -hidden jump %{
    evaluate-commands -save-regs a %{ # use evaluate-commands to ensure jumps are collapsed
        try %{
            evaluate-commands -draft %{
                execute-keys ',xs^([^:\n]+):(\d+):(\d+)?<ret>'
                set-register a %reg{1} %reg{2} %reg{3}
            }
            set-option global jump_current_buffer %val{bufname}
            set-option buffer jump_current_line %val{cursor_line}
            evaluate-commands -try-client %opt{jumpclient} -verbatim -- edit -existing -- %reg{a}
            try %{ focus %opt{jumpclient} }
        }
    }
}

define-command jump-next -params .. -docstring %{
    jump-next <bufname>: jump to next location listed in the given *grep*-like location list buffer.
} %{
    evaluate-commands -try-client %opt{jumpclient} -save-regs / %sh{
        args='%arg{@}'
        [ "$#" -eq 0 ] && args='%opt{jump_current_buffer}'
        printf %s\\n "buffer $args" jump-select-next jump
    }
    try %{
        evaluate-commands -client %opt{toolsclient} %{
            buffer %opt{jump_current_buffer}
            execute-keys gg %opt{jump_current_line}g
        }
    }
}
complete-command jump-next buffer
define-command -hidden jump-select-next %{
    # First jump to end of buffer so that if jump_current_line == 0
    # 0g<a-l> will be a no-op and we'll jump to the first result.
    # Yeah, thats ugly...
    execute-keys ge %opt{jump_current_line}g<a-l> /^[^:\n]+:\d+:<ret>
}

define-command jump-previous -params .. -docstring %{
    jump-previous <bufname>: jump to previous location listed in the given *grep*-like location list buffer.
} %{
    evaluate-commands -try-client %opt{jumpclient} -save-regs / %sh{
        args='%arg{@}'
        [ "$#" -eq 0 ] && args='%opt{jump_current_buffer}'
        printf %s\\n "buffer $args" jump-select-previous jump
    }
    try %{
        evaluate-commands -client %opt{toolsclient} %{
            buffer %opt{jump_current_buffer}
            execute-keys gg %opt{jump_current_line}g
        }
    }
}
complete-command jump-previous buffer
define-command -hidden jump-select-previous %{
    # See comment in jump-select-next
    execute-keys ge %opt{jump_current_line}g<a-h> <a-/>^[^:\n]+:\d+:<ret>
}

}

hook -once global KakBegin .* %{ require-module jump }
