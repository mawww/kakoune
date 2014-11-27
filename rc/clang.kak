decl str clang_options

decl -hidden str clang_tmp_dir
decl -hidden str-list clang_completions
decl -hidden line-flag-list clang_flags
decl -hidden str clang_errors

def -shell-params clang-parse %{
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
              }"
        # this runs in a detached shell, asynchronously, so that kakoune does
        # not hang while clang is running. As completions references a cursor
        # position and a buffer timestamp, only valid completions should be
        # displayed.
        (
            case ${kak_opt_filetype} in
                cpp) ft=c++ ;;
                obj-c) ft=objective-c ;;
                *) ft=c++ ;;
            esac

            cd $(dirname ${kak_buffile})
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
	                             if (id in completions)
	                                 completions[id]=completions[id] "\\n" desc
	                             else
	                                 completions[id]=desc
	                        }
	                        END {
	                            for (id in completions)
	                                print id  "@" completions[id]
	                        }' | sort | paste -s -d ':' | sed -e 's/\\n/\n/g')
	            echo "eval -client ${kak_client} echo completed
	                  set 'buffer=${kak_buffile}' clang_completions %[${header}:${compl}]" | kak -p ${kak_session}
            else
				clang++ -x ${ft} -fsyntax-only ${kak_opt_clang_options} - < ${dir}/buf 2> ${dir}/stderr
            fi

            flags=$(cat ${dir}/stderr | sed -rne "
                        /^<stdin>:[0-9]+:([0-9]+:)? error/ { s/^<stdin>:([0-9]+):.*/\1,red,█/; p }
                        /^<stdin>:[0-9]+:([0-9]+:)? warning/ { s/^<stdin>:([0-9]+):.*/\1,yellow,█/; p }
                    " | paste -s -d ':')

            errors=$(cat ${dir}/stderr | sed -rne "
                        /^<stdin>:[0-9]+:([0-9]+:)? (error|warning)/ { s/^<stdin>:([0-9]+):([0-9]+:)? (.*)/\1,\3/; p }")

            sed -e "s|<stdin>|${kak_bufname}|g" < ${dir}/stderr > ${dir}/fifo

            echo "set 'buffer=${kak_buffile}' clang_flags %{${flags}}
                  set 'buffer=${kak_buffile}' clang_errors %{${errors}}" | kak -p ${kak_session}

            rm -r ${dir}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

def clang-complete %{ clang-parse -complete }

def clang-enable-autocomplete %{
    set window completers "option=clang_completions:%opt{completers}"
    hook window -group clang-autocomplete InsertIdle .* %{ try %{
        exec -draft <a-h><a-k>(\.|->|::).\'<ret>
        echo 'completing...'
        clang-complete
    } }
    alias window complete clang-complete
}

def clang-disable-autocomplete %{
    set window completers %sh{ echo "'${kak_opt_completers}'" | sed -e 's/option=clang_completions://g' }
    rmhooks window clang-autocomplete
    unalias window complete clang-complete
}

def -hidden clang-show-error-info %{ %sh{
    echo "${kak_opt_clang_errors}" | while read line; do
        case "${line}" in
           ${kak_cursor_line},*) echo "info -anchor ${kak_cursor_line}.${kak_cursor_column} %{${line#*,}}" ;;
        esac
    done
} }

def clang-enable-diagnostics %{
    addhl flag_lines default clang_flags
    hook window -group clang-diagnostics NormalIdle .* %{ clang-show-error-info }
}

def clang-disable-diagnostics %{
    rmhl hlflags_clang_flags
    rmhooks window clang-diagnostics
}
