decl str clang_options

decl -hidden str clang_tmp_dir
decl -hidden str-list clang_completions
decl -hidden line-flags clang_flags
decl -hidden str clang_errors

def clang-parse -params 0..1 -docstring "Parse the contents of the current buffer with clang" %{
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
                  hook -group fifo buffer BufCloseFifo .* %{
                      nop %sh{ rm -r ${dir} }
                      rmhooks buffer fifo
                  }
              }"
        # this runs in a detached shell, asynchronously, so that kakoune does
        # not hang while clang is running. As completions references a cursor
        # position and a buffer timestamp, only valid completions should be
        # displayed.
        (
            case ${kak_opt_filetype} in
                c) ft=c ;;
                cpp) ft=c++ ;;
                obj-c) ft=objective-c ;;
                *) ft=c++ ;;
            esac

            if [ "$1" == "-complete" ]; then
                pos=-:${kak_cursor_line}:${kak_cursor_column}
                header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"
                compl=$(clang++ -x ${ft} -fsyntax-only ${kak_opt_clang_options} \
                    -Xclang -code-completion-brief-comments -Xclang -code-completion-at=${pos} - < ${dir}/buf 2> ${dir}/stderr |
                        awk -F ': ' -e '
                            /^COMPLETION:/ && ! /\(Hidden\)/ {
                                 gsub(/[[{<]#|#[}>]/, "", $3)
                                 gsub(/#]/, " ", $3)
                                 gsub(/:: /, "::", $3)
                                 gsub(/ +$/, "", $3)
                                 id=substr($2, 1, length($2)-1)
                                 gsub(/:/, "\\:", id)
                                 desc=$4 ? $3 "\\n" $4 : $3
                                 gsub(/:/, "\\:", desc)
                                 if (id in docstrings)
                                     docstrings[id]=docstrings[id] "\\n" desc
                                 else
                                     docstrings[id]=desc
                            }
                            END {
                                for (id in docstrings)
                                {
                                    menu=id
                                    gsub(/(^|[^[:alnum:]_])(operator|new|delete)($|[^[:alnum:]_])/, "{keyword}&{}", menu)
                                    gsub(/(^|[[:space:]])(int|size_t|bool|char|unsigned|signed|long)($|[[:space:]])/, "{type}&{}", menu)
                                    gsub(/[^[:alnum:]{}_]+/, "{operator}&{}", menu)
                                    print id  "@" docstrings[id] "@" menu
                                }
                            }' | sort | paste -s -d ':' | sed -e "s/\\\\n/\\n/g; s/'/\\\\'/g")
                echo "eval -client ${kak_client} echo 'clang completion done'
                      set 'buffer=${kak_buffile}' clang_completions '${header}:${compl}'" | kak -p ${kak_session}
            else
                clang++ -x ${ft} -fsyntax-only ${kak_opt_clang_options} - < ${dir}/buf 2> ${dir}/stderr
                echo "eval -client ${kak_client} echo 'clang parsing done'" | kak -p ${kak_session}
            fi

            flags=$(cat ${dir}/stderr | sed -rne "
                        /^<stdin>:[0-9]+:([0-9]+:)? (fatal )?error/ { s/^<stdin>:([0-9]+):.*/\1|{red}█/; p }
                        /^<stdin>:[0-9]+:([0-9]+:)? warning/ { s/^<stdin>:([0-9]+):.*/\1|{yellow}█/; p }
                    " | paste -s -d ':')

            errors=$(cat ${dir}/stderr | sed -rne "
                        /^<stdin>:[0-9]+:([0-9]+:)? ((fatal )?error|warning)/ { s/^<stdin>:([0-9]+):([0-9]+:)? (.*)/\1,\3/; s/'/\\\\'/g; p }
                     " | sort -n)

            sed -e "s|<stdin>|${kak_bufname}|g" < ${dir}/stderr > ${dir}/fifo

            echo "set 'buffer=${kak_buffile}' clang_flags %{${kak_timestamp}:${flags}}
                  set 'buffer=${kak_buffile}' clang_errors '${errors}'" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def clang-complete -docstring "Complete the current selection with clang" %{ clang-parse -complete }

def -hidden clang-show-completion-info %[ try %[
    eval -draft %[
        exec '<space>{(<a-k>^\(<ret>b'
        %sh[
            desc=$(echo "${kak_opt_clang_completions}" | sed -e 's/\([^\\]\):/\1\n/g; s/\\:/:/g' | grep "^${kak_selection}@" | head -n1 | sed -e 's/.*[^\\]@\(.*[^\\]\)@.*$/\1/' )
            if [ -n "$desc" ]; then
                echo "eval -client $kak_client %{info -anchor ${kak_cursor_line}.${kak_cursor_column} %{${desc}}}"
            fi
    ] ]
] ]

def clang-enable-autocomplete -docstring "Enable completion with clang" %{
    set window completers "option=clang_completions:%opt{completers}"
    hook window -group clang-autocomplete InsertIdle .* %{
        try %{
            exec -draft <a-h><a-k>(\.|->|::).\'<ret>
            echo 'completing...'
            clang-complete
        }
        clang-show-completion-info
    }
    alias window complete clang-complete
}

def clang-disable-autocomplete -docstring "Disable automatic clang completion" %{
    set window completers %sh{ echo "'${kak_opt_completers}'" | sed -e 's/option=clang_completions://g' }
    rmhooks window clang-autocomplete
    unalias window complete clang-complete
}

def -allow-override -hidden clang-show-error-info %{ %sh{
    echo "${kak_opt_clang_errors}" | grep "^${kak_cursor_line},.*" | if read line; then
        desc=$(echo ${line} | sed -e "s/^[[:digit:]]\+,//g; s/'/\\\\'/g")
        echo "info -anchor ${kak_cursor_line}.${kak_cursor_column} '${desc}'"
    fi
} }

def clang-enable-diagnostics -docstring "Activate automatic diagnostics of the code by clang" %{
    addhl flag_lines default clang_flags
    hook window -group clang-diagnostics NormalIdle .* %{ clang-show-error-info }
}

def clang-disable-diagnostics -docstring "Disable automatic diagnostics of the code" %{
    rmhl hlflags_clang_flags
    rmhooks window clang-diagnostics
}

def clang-diagnostics-next -docstring "Jump to the next line that contains an error" %{ %sh{
    echo "${kak_opt_clang_errors}" | (
        line=-1
        first_line=-1
        while read line_content; do
            candidate=${line_content%%,*}
            if [ -n "$candidate" ]; then
                first_line=$(( first_line == -1 ? candidate : first_line ))
                line=$((candidate > kak_cursor_line && (candidate < line || line == -1) ? candidate : line ))
            fi
        done
        line=$((line == -1 ? first_line : line))
        if [ ${line} -ne -1 ]; then
            echo "exec ${line} g"
        else
            echo 'echo -color Error no next clang diagnostic'
        fi
    )
} }
