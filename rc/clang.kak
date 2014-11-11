decl str clang_options

decl -hidden str clang_tmp_dir
decl -hidden str-list clang_completions

def clang-complete %{
    %sh{
        dir=$(mktemp -d -t kak-clang.XXXXXXXX)
        mkfifo ${dir}/fifo
        echo "set buffer clang_tmp_dir ${dir}"
        echo "write ${dir}/buf"
    }
    # end the previous %sh{} so that its output gets interpreted by kakoune
    # before launching the following as a background task.
    %sh{
        dir=${kak_opt_clang_tmp_dir}
        echo "eval -draft %{
                  edit! -fifo ${dir}/fifo *clang-output*
                  set buffer filetype make
                  set buffer _make_current_error_line 0
                  hook -group clang-cleanup buffer BufCloseFifo .* %{
                      nop %sh{ rm -r ${dir} }
                      rmhooks buffer clang-cleanup
                  }
              }"
        # this runs in a detached shell, asynchronously, so that kakoune does
        # not hang while clang is running. As completions references a cursor
        # position and a buffer timestamp, only valid completions should be
        # displayed.
        (
            pos=-:${kak_cursor_line}:${kak_cursor_column}
            cd $(dirname ${kak_buffile})
            header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"
            compl=$(clang++ -x c++ -fsyntax-only ${kak_opt_clang_options} -Xclang -code-completion-at=${pos} - < ${dir}/buf 2> ${dir}/fifo |
                    awk -F ': ' -e '
                        /^COMPLETION:/ && ! /\(Hidden\)/ {
                             gsub(/[[{<]#|#[}>]/, "", $3)
                             gsub(/#]/, " ", $3)
                             gsub(/:/, "\\:", $2)
                             gsub(/:/, "\\:", $3)
                             id=substr($2, 1, length($2)-1)
                             if (id in completions)
                                 completions[id]=completions[id] "\\n" $3
                             else
                                 completions[id]=$3
                        }
                        END {
                            for (id in completions)
                                print id  "@" completions[id]
                        }' | sort | paste -s -d ':' | sed -e 's/\\n/\n/g')

            echo "eval -client ${kak_client} %[ echo completed; set 'buffer=${kak_buffile}' clang_completions %[${header}:${compl}] ]" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def clang-enable-autocomplete %{
    set window completers %rec{option=clang_completions:%opt{completers}}
    hook window -group clang-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>(\.|->|::).\'<ret>
        echo 'completing...'
        clang-complete
    } }
}

def clang-disable-autocomplete %{
    set window completers %sh{ echo "'${kak_opt_completers}'" | sed -e 's/option=clang_completions://g' }
    rmhooks window clang-autocomplete
}
