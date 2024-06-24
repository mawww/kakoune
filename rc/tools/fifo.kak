provide-module fifo %{

define-command -params .. -docstring %{
    fifo [-name <name>] [-scroll] [-script <script>] [--] <args>...: run command in a fifo buffer
    if <script> is used, eval it with <args> as '$@', else pass arguments directly to the shell
} fifo %{ evaluate-commands %sh{
    name='*fifo*'
    while true; do
        case "$1" in
            "-scroll") scroll="-scroll"; shift ;;
            "-script") script="$2"; shift 2 ;;
            "-name") name="$2"; shift 2 ;;
            "--") shift; break ;;
            *) break ;;
        esac
    done
    output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-fifo.XXXXXXXX)/fifo
    mkfifo ${output}
    if [ -n "$script" ]; then
        ( eval "$script" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null
    else
        ( "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null
    fi

    printf %s\\n "
            edit! -fifo ${output} ${scroll} ${name}
            hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
        "
    }}

complete-command fifo shell

}

hook -once global KakBegin .* %{ require-module fifo }
