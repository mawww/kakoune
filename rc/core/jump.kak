
declare-option -docstring "name of the client in which all source code jumps will be executed" \
    str jumpclient

define-command -params 1..3 \
    -docstring %{jump [<switches>] <file> [<line> [<column>]]: open and focus file

Accepts all switches which can be passed to edit.} \
    -file-completion \
    jump %{
    evaluate-commands -try-client %opt{jumpclient} %{
        edit %arg{@}
        try %{ focus }
    }
}
