decl -hidden str racer_tmp_dir
decl -hidden completions racer_completions

def racer-complete -docstring "Complete the current selection with racer" %{
    %sh{
        dir=$(mktemp -d -t kak-racer.XXXXXXXX)
        printf %s\\n "set buffer racer_tmp_dir ${dir}"
        printf %s\\n "eval -no-hooks %{ write ${dir}/buf }"
    }
    %sh{
        dir=${kak_opt_racer_tmp_dir}
        (
            cursor="${kak_cursor_line} $((${kak_cursor_column} - 1))"
            racer_data=$(racer --interface tab-text complete-with-snippet ${cursor} ${kak_buffile} ${dir}/buf)
            compl=$(printf %s\\n "${racer_data}" | awk '
                BEGIN { FS = "\t"; ORS = ":" }
                /^PREFIX/ {
                    column = ENVIRON["kak_cursor_column"] + $2 - $3
                    print ENVIRON["kak_cursor_line"] "." column "\\@" ENVIRON["kak_timestamp"]
                }
                /^MATCH/ {
                    word = $2
                    type = $7
                    desc = substr($9, 2, length($9) - 2)
                    gsub(/\|/, "\\|", desc)
                    gsub(/\\n/, "\n", desc)
                    menu = $8
                    sub(/^pub /, "", menu)
                    gsub(/\|/, "\\|", menu)
                    if (type == "Function") {
                        sub(word, "{default+e}" word "{default+d}", menu)
                        menu = "{default+d}" menu
                        word = word "("
                    } else if (type == "Enum") {
                        menu = substr(menu, 0, length(menu) - 2)
                        sub(word, "{default+e}" word "{default+d}", menu)
                        menu = "{default+d}" menu
                        word = word "::"
                    } else {
                        menu = "{default+e}" word "{default+d} " menu
                    }
                    candidate = word "|" desc "|" menu
                    gsub(/:/, "\\:", candidate)
                    print candidate
                }'
            )
            printf %s\\n "eval -client '${kak_client}' %{
                set buffer=${kak_bufname} racer_completions %@${compl}@
            }" | kak -p ${kak_session}
            rm -r ${dir}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def racer-enable-autocomplete -docstring "Add racer completion candidates to the completer" %{
    set window completers "option=racer_completions:%opt{completers}"
    hook window -group racer-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>([\w\.]|::).\z<ret>
        racer-complete
    } }
    alias window complete racer-complete
}

def racer-disable-autocomplete -docstring "Disable racer completion" %{
    set window completers %sh{ printf %s\\n "'${kak_opt_completers}'" | sed 's/option=racer_completions://g' }
    rmhooks window racer-autocomplete
    unalias window complete racer-complete
}
