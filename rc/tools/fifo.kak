provide-module fifo %{

define-command -params .. -docstring %{
    fifo [-name <buffer-name>] [-scroll] [--] <command>...: run command in a fifo buffer
} fifo %{ evaluate-commands %sh{
    name='*fifo*'
    while true; do
        case "$1" in
            "-scroll") scroll="-scroll"; shift ;;
            "-name") name="$2"; shift 2 ;;
            "--") shift; break ;;
            *) break ;;
        esac
    done
    output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-fifo.XXXXXXXX)/fifo
    mkfifo ${output}
    ( eval "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

    printf %s\\n "
            edit! -fifo ${output} ${scroll} ${name}
            hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
        "
    }}

complete-command fifo shell

}

hook -once global KakBegin .* %{ require-module fifo }
